#ifndef UTIL_H
#define UTIL_H


#include <QChar>
#include <QString>

namespace util {

inline QString yesNo(bool b) { return b ? "yes" : "no"; }

inline bool isWhole(QChar w) { return w != '.' && w != '*'; }

template<class T> T sq(T v) { return v * v; }

}

#endif
