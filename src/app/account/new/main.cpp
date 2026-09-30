//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: main.cpp,v 1.47 2022/04/08 15:15:36 build Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

//
#include "audef.h"
#include "auutil.h"
#include "aulogincheck.h"
#include <iostream>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <QUrl>

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#define g_AppName						"Shores of Hazeron"

// Commonly used HTML fragments.
static const char* s_htmlHeader = "<html>";
static const char* s_bodyStart = "<body style=\"color: rgb(112, 127, 255); background-color: rgb(0, 0, 0); font-family: Helvetica, Arial, sans-serif;\" alink=\"#ffffff\" lang=\"EN-US\" link=\"#a0b3f9\" vlink=\"#a0a0a0\">"

// This part is the contents of sohpageheader.html. The php include did not work here. Hm.
"<table border=\"0\" cellpadding=\"0\" cellspacing=\"0\">"
"<tbody>"
"<tr>"
"<td>"
"<a href=\"https://Hazeron.com\" target=\"_top\"><img align=\"left\" title=\"Shores of Hazeron Home\" alt=\"Shores of Hazeron\" src=\"https://hazeron.com/images/HSSPageHeader.png\" style=\"border: 0px solid; width: 1000px;\"></a>"
"</td>"
"<tr>"
"</tbody>"
"</table><br>";

static const char* s_htmlFooter = "</body></html>";

// Function Prototypes
int CreateUserAccount(const QString& acctname, const QString& password1, const QString& password2, const QString& email);
void ErrorBadAddress(const QString& address);
void ErrorBlankAddress();
void ErrorBlankName();
void ErrorBlankPassword();
void ErrorPasswordMismatch();
void ErrorDuplicateName(const QString& acctname);
void GoodReply(const QString& acctname, const QString& email);
void NewAccountsDisabled();
void NewAccountsUnavailable();

int main(int argc, char** argv)
{
	QCoreApplication a(argc, argv);
	if(argc > 1)
	{
		QStringList args = a.arguments();
		if(args.contains("-version") || args.contains("-v"))
		{
			// Display version information and then exit.
			std::cout << "AuAcctNew " << (const char*)AuUtil::VersionFull().toLatin1() << '\n';
			exit(0);
		}
	}

	QSettings settings("AuAcctNew.ini", QSettings::IniFormat);
	int isEnabled = settings.value("Settings/enable", 0).toInt();
	if(qEnvironmentVariableIsSet("AU_ACCOUNT_REGISTRATION"))
		isEnabled = qEnvironmentVariableIntValue("AU_ACCOUNT_REGISTRATION");
	if(!isEnabled)
	{
		NewAccountsDisabled();
		return 0;
	}

	const QByteArray method = qgetenv("REQUEST_METHOD");
	if(!method.isEmpty() && method != "POST")
	{
		printf("Status: 405 Method Not Allowed\r\nAllow: POST\r\nContent-Type: text/plain\r\n\r\nMethod not allowed.\n");
		return 0;
	}

	// Read the POST data from standard input.
	QTextStream stream(stdin);
	QString line;
	line = stream.readLine();
	if(!line.isEmpty())
	{
		// Parse the line into "a=b" fields. They are delimited with ampersands.
		// e.g. "a=b&name=Bob%20Jones&state=CO"
		// e.g. "acctname=Bob&password1=bob&password2=bob&email=bob@here.com"
		QString acctname, password1, password2, email;
		QStringList fields = line.split('&');
		for(QStringList::const_iterator fit = fields.constBegin(); fit != fields.constEnd(); fit++)
		{
			// Parse the "a=b" field into two values.
			QStringList fieldVals = (*fit).split('=');
			if(fieldVals.count() == 2)
			{
				QString name = fieldVals[0];	// field name
				QString val = fieldVals[1];		// field value
				val = QUrl::fromPercentEncoding(val.replace("+", " ").toUtf8());

				// Collect field values.
				if(name == "acctname")
					acctname = val.trimmed();
				else if(name == "password1")
					password1 = val;
				else if(name == "password2")
					password2 = val;
				else if(name == "email")
					email = val.trimmed();
			}
		}

		CreateUserAccount(acctname, password1, password2, email);
	}
	else
	{
		ErrorBlankName();
	}

	return 0;
}

// returns:
// 0 = ok
// 1 = blank name
// 2 = blank password
// 3 = password mismatch
// 4 = blank email address
// 5 = bad email address
// 6 = new accounts disabled
// 7 = account name already exists
// 8 = unexpected error
int CreateUserAccount(const QString& acctname, const QString& password1, const QString& password2, const QString& email)
{
	// Validate the field contents.
	if(acctname.isEmpty())
	{
		ErrorBlankName();
		return 1;
	}
	else if(password1.isEmpty())
	{
		ErrorBlankPassword();
		return 2;
	}
	else if(password1 != password2)
	{
		ErrorPasswordMismatch();
		return 3;
	}
	else if(email.isEmpty())
	{
		ErrorBlankAddress();
		return 4;
	}
	else if(email.count('@') != 1 ||
			email.count(' ') > 0 ||
			email.count('.') == 0)
	{
		ErrorBadAddress(email);
		return 5;
	}

	// Login to the login server.
	QString loginName = "WebMaster.Hazeron.com";

	// Create a SHA-1 hash key from the webmaster's password.
	QByteArray loginPassCrypt;
	loginPassCrypt = qgetenv("AU_WEBMASTER_PASSWORD");
	if(loginPassCrypt.isEmpty())
	{
		NewAccountsUnavailable();
		return 6;
	}
	QCryptographicHash loginPassCryptHash(QCryptographicHash::Sha1);
	loginPassCryptHash.addData(loginPassCrypt);
	loginPassCrypt = loginPassCryptHash.result();

	AuLoginCheck login;
	int result = login.LoginTestCredentials(loginName, loginPassCrypt);
	//qDebug() << "Login result" << result;
	if(!result)
	{
		// Attempt to create the account and wait for a response.
		// Create a SHA-1 hash key from the password.
		QByteArray passCrypt;
		passCrypt.append(password1.toUtf8());
		QCryptographicHash passCryptHash(QCryptographicHash::Sha1);
		passCryptHash.addData(passCrypt);
		passCrypt = passCryptHash.result();
#if defined(Free2Play)
		ACCOUNT status = ACCT_Active;		// Free2Play accounts are created fully active.
#else
		//ACCOUNT status = ACCT_LockedOut;	// Accounts are created locked until activated.
		ACCOUNT status = ACCT_Trial;		// Trial accounts are created with a few days of free activation time.
#endif
		result = login.CreateAccount(acctname, passCrypt, email, status);
		//qDebug() << "CreateAccount result" << result;
		switch(result)
		{
		case 0:
			// It looks like all went well.
			GoodReply(acctname, email);
			return 0;
		case 1:
			// An unspecified error occurred.
			NewAccountsUnavailable();
			return 6;
		case 5:
			// The account name already exists.
			ErrorDuplicateName(acctname);
			return 7;
		default:
			// Unexpected error.
			NewAccountsUnavailable();
			return 8;
		}
	}
	else
	{
		NewAccountsUnavailable();
		return 6;
	}
}

void GoodReply(const QString& acctname, const QString& email)
{
	// This HTML header is required to be the first output to stdout from our
	// cgi program. The HTML header(s) must terminate with a blank line.
	printf("Content-Type: text/html; charset=UTF-8\r\nCache-Control: no-store\r\n\r\n");

	// The following code simply constructs a web page.
	const char* titleBar = "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"><title>" g_AppName " - Account Created</title></head>\n<link rel=\"shortcut icon\" href=\"https://hazeron.com/favicon.png\" type=\"image/png\">\n";
	const char* pageTitle = "<div style=\"text-align: center;\"><big style=\"font-weight: bold;\">" g_AppName " Account Created</big></div><br>\n";

	//printf(s_htmlHeader);
	//printf(titleBar);
	//printf(s_bodyStart);
	//printf(pageTitle);
	puts(s_htmlHeader);
	puts(titleBar);
	puts(s_bodyStart);
	puts(pageTitle);

#if defined(Free2Play)
	printf("<p>Your new login account is ready.</p><p> Here is your login information.</p>\n");
#else
	printf("<p>Your new login account is set up <b>with a one week free trial</b>. "
			"</p>"
			"<p>Here is your login information.<p>\n");
#endif
	printf("<p><b>Print this page and retain a copy for your records.</b></p>\n");
	printf("<div style=\"margin-left: 40px;\"><font color=\"#a86d30\">Account Name: <b>%s</b><br>",
			acctname.toHtmlEscaped().toUtf8().constData());
	printf("E-mail Address: <b>%s</b><p></font></div>", email.toHtmlEscaped().toUtf8().constData());

	//printf(s_htmlFooter);
	puts(s_htmlFooter);
}

void ErrorBadAddress(const QString& address)
{
	// This HTML header is required to be the first output to stdout from our
	// cgi program. The HTML header(s) must terminate with a blank line.
	printf("Content-Type: text/html; charset=UTF-8\r\nCache-Control: no-store\r\n\r\n");

	// The following code simply constructs a web page.
	const char* titleBar = "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"><title>" g_AppName " - Bad E-mail Address</title></head>\n<link rel=\"shortcut icon\" href=\"https://hazeron.com/favicon.png\" type=\"image/png\">\n";
	const char* pageTitle = "<div style=\"text-align: center;\"><big style=\"font-weight: bold;\">Bad E-mail Address</big></div><br>\n";

	//printf(s_htmlHeader);
	//printf(titleBar);
	//printf(s_bodyStart);
	//printf(pageTitle);
	puts(s_htmlHeader);
	puts(titleBar);
	puts(s_bodyStart);
	puts(pageTitle);

	printf("There was a problem with the e-mail address entered.<p>\n");
	printf("<div style=\"margin-left: 40px;\">E-mail Address: <b>%s</b><p></div>", address.toHtmlEscaped().toUtf8().constData());
	printf("Please go back and enter a valid e-mail address.<p>\n");

	//printf(s_htmlFooter);
	puts(s_htmlFooter);
}

void ErrorBlankAddress()
{
	// This HTML header is required to be the first output to stdout from our
	// cgi program. The HTML header(s) must terminate with a blank line.
	printf("Content-Type: text/html; charset=UTF-8\r\nCache-Control: no-store\r\n\r\n");

	// The following code simply constructs a web page.
	const char* titleBar = "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"><title>" g_AppName " - Blank E-mail Address</title></head>\n<link rel=\"shortcut icon\" href=\"https://hazeron.com/favicon.png\" type=\"image/png\">\n";
	const char* pageTitle = "<div style=\"text-align: center;\"><big style=\"font-weight: bold;\">Blank E-mail Address</big></div><br>\n";

	//printf(s_htmlHeader);
	//printf(titleBar);
	//printf(s_bodyStart);
	//printf(pageTitle);
	puts(s_htmlHeader);
	puts(titleBar);
	puts(s_bodyStart);
	puts(pageTitle);

	printf("The e-mail address must not be blank. Your e-mail address is needed so we can communicate with you regarding your account. This is especially helpful if you ever forget your password.<p>\n");
	printf("Please go back and enter a valid e-mail address.<p>\n");

	//printf(s_htmlFooter);
	puts(s_htmlFooter);
}

void ErrorBlankName()
{
	// This HTML header is required to be the first output to stdout from our
	// cgi program. The HTML header(s) must terminate with a blank line.
	printf("Content-Type: text/html; charset=UTF-8\r\nCache-Control: no-store\r\n\r\n");

	// The following code simply constructs a web page.
	const char* titleBar = "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"><title>" g_AppName " - Blank Account Name</title></head>\n<link rel=\"shortcut icon\" href=\"https://hazeron.com/favicon.png\" type=\"image/png\">\n";
	const char* pageTitle = "<div style=\"text-align: center;\"><big style=\"font-weight: bold;\">Blank Account Name</big></div><br>\n";

	//printf(s_htmlHeader);
	//printf(titleBar);
	//printf(s_bodyStart);
	//printf(pageTitle);
	puts(s_htmlHeader);
	puts(titleBar);
	puts(s_bodyStart);
	puts(pageTitle);

	printf("The account name must not be blank. An account name is needed to login to " g_AppName " when the program starts.<p>\n");
	printf("Please go back and enter an account name.<p>\n");

	//printf(s_htmlFooter);
	puts(s_htmlFooter);
}

void ErrorBlankPassword()
{
	// This HTML header is required to be the first output to stdout from our
	// cgi program. The HTML header(s) must terminate with a blank line.
	printf("Content-Type: text/html; charset=UTF-8\r\nCache-Control: no-store\r\n\r\n");

	// The following code simply constructs a web page.
	const char* titleBar = "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"><title>" g_AppName " - Blank Password</title></head>\n<link rel=\"shortcut icon\" href=\"https://hazeron.com/favicon.png\" type=\"image/png\">\n";
	const char* pageTitle = "<div style=\"text-align: center;\"><big style=\"font-weight: bold;\">Blank Password</big></div><br>\n";

	//printf(s_htmlHeader);
	//printf(titleBar);
	//printf(s_bodyStart);
	//printf(pageTitle);
	puts(s_htmlHeader);
	puts(titleBar);
	puts(s_bodyStart);
	puts(pageTitle);

	printf("The password must not be blank. A password is needed to login to " g_AppName " when the program starts.<p>\n");
	printf("Please go back and enter a password.<p>\n");

	//printf(s_htmlFooter);
	puts(s_htmlFooter);
}

void ErrorPasswordMismatch()
{
	// This HTML header is required to be the first output to stdout from our
	// cgi program. The HTML header(s) must terminate with a blank line.
	printf("Content-Type: text/html; charset=UTF-8\r\nCache-Control: no-store\r\n\r\n");

	// The following code simply constructs a web page.
	const char* titleBar = "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"><title>" g_AppName " - Password Mismatch</title></head>\n<link rel=\"shortcut icon\" href=\"https://hazeron.com/favicon.png\" type=\"image/png\">\n";
	const char* pageTitle = "<div style=\"text-align: center;\"><big style=\"font-weight: bold;\">Password Mismatch</big></div><br>\n";

	//printf(s_htmlHeader);
	//printf(titleBar);
	//printf(s_bodyStart);
	//printf(pageTitle);
	puts(s_htmlHeader);
	puts(titleBar);
	puts(s_bodyStart);
	puts(pageTitle);

	printf("The password does not match the confirmation password.<p>\n");
	printf("Please go back and enter a password and a confirmation password that are the same.<p>\n");

	//printf(s_htmlFooter);
	puts(s_htmlFooter);
}

void ErrorDuplicateName(const QString& name)
{
	// This HTML header is required to be the first output to stdout from our
	// cgi program. The HTML header(s) must terminate with a blank line.
	printf("Content-Type: text/html; charset=UTF-8\r\nCache-Control: no-store\r\n\r\n");

	// The following code simply constructs a web page.
	const char* titleBar = "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"><title>" g_AppName " - Account Name In Use</title></head>\n<link rel=\"shortcut icon\" href=\"https://hazeron.com/favicon.png\" type=\"image/png\">\n";
	const char* pageTitle = "<div style=\"text-align: center;\"><big style=\"font-weight: bold;\">Account Name In Use</big></div><br>\n";

	//printf(s_htmlHeader);
	//printf(titleBar);
	//printf(s_bodyStart);
	//printf(pageTitle);
	puts(s_htmlHeader);
	puts(titleBar);
	puts(s_bodyStart);
	puts(pageTitle);

	printf("The account name you chose is already in use. Your account name must be unique among all players.<p>\n");
	printf("<div style=\"margin-left: 40px;\">Account Name: <b>%s</b><p></div>", name.toHtmlEscaped().toUtf8().constData());
	printf("We apologize for the inconvenience.<p>\n");
	printf("Please go back and enter a different account name.<p>\n");

	//printf(s_htmlFooter);
	puts(s_htmlFooter);
}

void NewAccountsDisabled()
{
	// This HTML header is required to be the first output to stdout from our
	// cgi program. The HTML header(s) must terminate with a blank line.
	printf("Content-Type: text/html; charset=UTF-8\r\nCache-Control: no-store\r\n\r\n");

	// The following code simply constructs a web page.
	const char* titleBar = "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"><title>" g_AppName " - New Accounts Disabled</title></head>\n<link rel=\"shortcut icon\" href=\"https://hazeron.com/favicon.png\" type=\"image/png\">\n";
	const char* pageTitle = "<div style=\"text-align: center;\"><big style=\"font-weight: bold;\">New Accounts Disabled</big></div><br>\n";

	//printf(s_htmlHeader);
	//printf(titleBar);
	//printf(s_bodyStart);
	//printf(pageTitle);
	puts(s_htmlHeader);
	puts(titleBar);
	puts(s_bodyStart);
	puts(pageTitle);

	printf("We are not accepting new " g_AppName " accounts at this time.<p>\n");
	printf("We apologize for the inconvenience. Please try again later.<p>\n");

	//printf(s_htmlFooter);
	puts(s_htmlFooter);
}

void NewAccountsUnavailable()
{
	// This HTML header is required to be the first output to stdout from our
	// cgi program. The HTML header(s) must terminate with a blank line.
	printf("Content-Type: text/html; charset=UTF-8\r\nCache-Control: no-store\r\n\r\n");

	// The following code simply constructs a web page.
	const char* titleBar = "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"><title>" g_AppName " - New Accounts Unavailable</title></head>\n<link rel=\"shortcut icon\" href=\"https://hazeron.com/favicon.png\" type=\"image/png\">\n";
	const char* pageTitle = "<div style=\"text-align: center;\"><big style=\"font-weight: bold;\">New Accounts Unavailable</big></div><br>\n";

	//printf(s_htmlHeader);
	//printf(titleBar);
	//printf(s_bodyStart);
	//printf(pageTitle);
	puts(s_htmlHeader);
	puts(titleBar);
	puts(s_bodyStart);
	puts(pageTitle);

	printf("Due to technical difficulties, a new " g_AppName " account cannot be created at this time.<p>\n");
	printf("We apologize for the inconvenience. Please try again later.<p>\n");

	//printf(s_htmlFooter);
	puts(s_htmlFooter);
}
