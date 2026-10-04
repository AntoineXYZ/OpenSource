//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: Spacecraft.f.glsl,v 1.4 2015/08/01 00:08:17 daveh Exp $
// Copyright (c) 2012 Software Engineering, Inc.
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#version 150

uniform float							discardAlpha;			// Alpha below which we discard fragments.
uniform sampler2D						texEffect;				// Texture for affects applied to the hull.
uniform sampler2D						texShip;				// Texture for the spacecraft hull.

in vec4									vertColor;				// Incoming fragment color interpolated from the vertex color.
in vec2									vertTexCoord;			// Incoming texture coordinate (xy)
in vec4									vertEffCoord;			// Incoming effect texture coordinate lit(xy), unlit(zw)

out vec4								fragColor;				// Outgoing fragment color.

void main()
{
	fragColor = texture(texShip, vertTexCoord);
	if(fragColor.a <= discardAlpha)
		discard;

	// Blend in the "lit" effect texture color before modulating with light.
	if(vertEffCoord.x >= 0)
	{
		vec4 litEffect = texture(texEffect, vertEffCoord.xy);	// Effect texture color.
		litEffect.a *= max(sign(vertEffCoord.x), 0);
		fragColor.rgb = mix(fragColor.rgb, litEffect.rgb, litEffect.a);
	}

	// Modulate using light-modulated color.
	fragColor *= vertColor;

	// Blend in the "unlit" effect texture color after light modulation applied.
	if(vertEffCoord.z >= 0)
	{
		vec4 unlitEffect = texture(texEffect, vertEffCoord.zw);	// Effect texture color.
		unlitEffect.a *= max(sign(vertEffCoord.z), 0);
		fragColor.rgb = mix(fragColor.rgb, unlitEffect.rgb, unlitEffect.a);
	}
}
