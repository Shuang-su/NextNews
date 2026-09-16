#pragma once
// Adapted from PlayCanvas Engine (MIT); see docs/harmonyos/THIRD_PARTY_NOTICES.md.
namespace splat {
inline const char* PostTone=R"POST(
vec3 toneMap0(vec3 color) {
	return color;
}


vec3 toneMap1(vec3 color) {
	return color * 1.0;
}


const float A =  0.15;
const float B =  0.50;
const float C =  0.10;
const float D =  0.20;
const float E =  0.02;
const float F =  0.30;
const float W =  11.2;
vec3 uncharted2Tonemap(vec3 x) {
	 return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}
vec3 toneMap2(vec3 color) {
	color = uncharted2Tonemap(color * 1.0);
	vec3 whiteScale = 1.0 / uncharted2Tonemap(vec3(W,W,W));
	color = color * whiteScale;
	return color;
}


vec3 toneMap3(vec3 color) {
	color *= 1.0;
	const float  A = 0.22, B = 0.3, C = .1, D = 0.2, E = .01, F = 0.3;
	const float Scl = 1.25;
	vec3 h = max( vec3(0.0), color - vec3(0.004) );
	return (h*((Scl*A)*h+Scl*vec3(C*B,C*B,C*B))+Scl*vec3(D*E,D*E,D*E)) / (h*(A*h+vec3(B,B,B))+vec3(D*F,D*F,D*F)) - Scl*vec3(E/F,E/F,E/F);
}


vec3 toneMap4(vec3 color) {
	float tA = 2.51;
	float tB = 0.03;
	float tC = 2.43;
	float tD = 0.59;
	float tE = 0.14;
	vec3 x = color * 1.0;
	return (x*(tA*x+tB))/(x*(tC*x+tD)+tE);
}


const mat3 ACESInputMat = mat3(
	0.59719, 0.35458, 0.04823,
	0.07600, 0.90834, 0.01566,
	0.02840, 0.13383, 0.83777
);
const mat3 ACESOutputMat = mat3(
	 1.60475, -0.53108, -0.07367,
	-0.10208,  1.10813, -0.00605,
	-0.00327, -0.07276,  1.07602
);
vec3 RRTAndODTFit(vec3 v) {
	vec3 a = v * (v + 0.0245786) - 0.000090537;
	vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
	return a / b;
}
vec3 toneMap5(vec3 color) {
	color *= 1.0 / 0.6;
	color = color * ACESInputMat;
	color = RRTAndODTFit(color);
	color = color * ACESOutputMat;
	color = clamp(color, 0.0, 1.0);
	return color;
}


vec3 toneMap6(vec3 color) {
	color *= 1.0;
	float startCompression = 0.8 - 0.04;
	float desaturation = 0.15;
	float x = min(color.r, min(color.g, color.b));
	float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
	color -= offset;
	float peak = max(color.r, max(color.g, color.b));
	if (peak < startCompression) return color;
	float d = 1. - startCompression;
	float newPeak = 1. - d * d / (peak + d - startCompression);
	color *= newPeak / peak;
	float g = 1. - 1. / (desaturation * (peak - newPeak) + 1.);
	return mix(color, newPeak * vec3(1, 1, 1), g);
}

)POST";
inline const char* PostVertex=R"POST(#version 300 es
precision highp float;
out vec2 uv0;
void main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));uv0=p;gl_Position=vec4(p*2.0-1.0,0,1);}
)POST";
inline const char* PostCompose=R"POST(#version 300 es
precision highp float;
out vec4 outColor;
in vec2 uv0;
uniform sampler2D sceneTexture;
uniform vec2 sceneTextureInvRes;
uniform float settings[22];

vec3 toneMap0(vec3 color) {
	return color;
}


vec3 toneMap1(vec3 color) {
	return color * 1.0;
}


const float A =  0.15;
const float B =  0.50;
const float C =  0.10;
const float D =  0.20;
const float E =  0.02;
const float F =  0.30;
const float W =  11.2;
vec3 uncharted2Tonemap(vec3 x) {
	 return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}
vec3 toneMap2(vec3 color) {
	color = uncharted2Tonemap(color * 1.0);
	vec3 whiteScale = 1.0 / uncharted2Tonemap(vec3(W,W,W));
	color = color * whiteScale;
	return color;
}


vec3 toneMap3(vec3 color) {
	color *= 1.0;
	const float  A = 0.22, B = 0.3, C = .1, D = 0.2, E = .01, F = 0.3;
	const float Scl = 1.25;
	vec3 h = max( vec3(0.0), color - vec3(0.004) );
	return (h*((Scl*A)*h+Scl*vec3(C*B,C*B,C*B))+Scl*vec3(D*E,D*E,D*E)) / (h*(A*h+vec3(B,B,B))+vec3(D*F,D*F,D*F)) - Scl*vec3(E/F,E/F,E/F);
}


vec3 toneMap4(vec3 color) {
	float tA = 2.51;
	float tB = 0.03;
	float tC = 2.43;
	float tD = 0.59;
	float tE = 0.14;
	vec3 x = color * 1.0;
	return (x*(tA*x+tB))/(x*(tC*x+tD)+tE);
}


const mat3 ACESInputMat = mat3(
	0.59719, 0.35458, 0.04823,
	0.07600, 0.90834, 0.01566,
	0.02840, 0.13383, 0.83777
);
const mat3 ACESOutputMat = mat3(
	 1.60475, -0.53108, -0.07367,
	-0.10208,  1.10813, -0.00605,
	-0.00327, -0.07276,  1.07602
);
vec3 RRTAndODTFit(vec3 v) {
	vec3 a = v * (v + 0.0245786) - 0.000090537;
	vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
	return a / b;
}
vec3 toneMap5(vec3 color) {
	color *= 1.0 / 0.6;
	color = color * ACESInputMat;
	color = RRTAndODTFit(color);
	color = color * ACESOutputMat;
	color = clamp(color, 0.0, 1.0);
	return color;
}


vec3 toneMap6(vec3 color) {
	color *= 1.0;
	float startCompression = 0.8 - 0.04;
	float desaturation = 0.15;
	float x = min(color.r, min(color.g, color.b));
	float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
	color -= offset;
	float peak = max(color.r, max(color.g, color.b));
	if (peak < startCompression) return color;
	float d = 1. - startCompression;
	float newPeak = 1. - d * d / (peak + d - startCompression);
	color *= newPeak / peak;
	float g = 1. - 1. / (desaturation * (peak - newPeak) + 1.);
	return mix(color, newPeak * vec3(1, 1, 1), g);
}

#define CAS
#define CAS_HDR

	#ifdef CAS
		uniform float sharpness;
		#ifdef CAS_HDR
			float maxComponent(float x, float y, float z) { return max(x, max(y, z)); }
			vec3 toSDR(vec3 c) { return settings[1]>0.0 ? c / (1.0 + maxComponent(c.r, c.g, c.b)) : c; }
			vec3 toHDR(vec3 c) { return settings[1]>0.0 ? c / max(1.0 - maxComponent(c.r, c.g, c.b), 1e-4) : c; }
		#else
			vec3 toSDR(vec3 c) { return c; }
			vec3 toHDR(vec3 c) { return c; }
		#endif
		vec3 applyCas(vec3 color, vec2 uv, float sharpness) {
			float x = sceneTextureInvRes.x;
			float y = sceneTextureInvRes.y;
			vec3 a = toSDR(textureLod(sceneTexture, uv + vec2(0.0, -y), 0.0).rgb);
			vec3 b = toSDR(textureLod(sceneTexture, uv + vec2(-x, 0.0), 0.0).rgb);
			vec3 c = toSDR(color.rgb);
			vec3 d = toSDR(textureLod(sceneTexture, uv + vec2(x, 0.0), 0.0).rgb);
			vec3 e = toSDR(textureLod(sceneTexture, uv + vec2(0.0, y), 0.0).rgb);
			float min_g = min(a.g, min(b.g, min(c.g, min(d.g, e.g))));
			float max_g = max(a.g, max(b.g, max(c.g, max(d.g, e.g))));
			float sharpening_amount = sqrt(min(1.0 - max_g, min_g) / max(max_g, 1e-4));
			float w = sharpening_amount * sharpness;
			vec3 res = (w * (a + b + d + e) + c) / (4.0 * w + 1.0);
			res = max(res, 0.0);
			return toHDR(res);
		}
	#endif

#define FRINGING

	#ifdef FRINGING
		uniform float fringingIntensity;
		vec3 applyFringing(vec3 color, vec2 uv) {
			vec2 centerDistance = uv - 0.5;
			vec2 offset = fringingIntensity * centerDistance * centerDistance;
			color.r = texture(sceneTexture, uv - offset).r;
			color.b = texture(sceneTexture, uv + offset).b;
			return color;
		}
	#endif

#define GRADING

	#ifdef GRADING
		uniform vec3 brightnessContrastSaturation;
		uniform vec3 tint;
		vec3 colorGradingHDR(vec3 color, float brt, float sat, float con) {
			color *= tint;
			color = color * brt;
			float grey = dot(color, vec3(0.3, 0.59, 0.11));
			grey = grey / max(1.0, max(color.r, max(color.g, color.b)));
			color = mix(vec3(grey), color, sat);
			return mix(vec3(0.5), color, con);
		}
		vec3 applyGrading(vec3 color) {
			return colorGradingHDR(color, 
				brightnessContrastSaturation.x, 
				brightnessContrastSaturation.z, 
				brightnessContrastSaturation.y);
		}
	#endif

#define VIGNETTE

	#ifdef VIGNETTE
		uniform vec4 vignetterParams;
		uniform vec3 vignetteColor;
		
		float dVignette;
		
		float calcVignette(vec2 uv) {
			float inner = vignetterParams.x;
			float outer = vignetterParams.y;
			float curvature = vignetterParams.z;
			float intensity = vignetterParams.w;
			vec2 curve = pow(abs(uv * 2.0 -1.0), vec2(1.0 / curvature));
			float edge = pow(length(curve), curvature);
			dVignette = 1.0 - intensity * smoothstep(inner, outer, edge);
			return dVignette;
		}
		vec3 applyVignette(vec3 color, vec2 uv) {
			return mix(vignetteColor, color, calcVignette(uv));
		}
	#endif


uniform sampler2D bloomTexture;
void main(){
 vec3 result=texture(sceneTexture,uv0).rgb;
 if(settings[2]>0.0)result=applyCas(result,uv0,sharpness);
 if(settings[3]>0.0)result=applyFringing(result,uv0);
 if(settings[4]>0.0 && settings[1]>0.0)result+=texture(bloomTexture,uv0).rgb*settings[5];
 if(settings[7]>0.0)result=applyGrading(result);
 result=max(result,vec3(0));
 int mode=int(settings[0]);
 if(mode==2)result=toneMap2(result);else if(mode==3)result=toneMap3(result);
 else if(mode==4)result=toneMap4(result);else if(mode==5)result=toneMap5(result);else if(mode==6)result=toneMap6(result);
 if(settings[14]>0.0)result=applyVignette(result,uv0);
 // MetaFlow configureCamera forces GAMMA_NONE on the final compose blit.
 outColor=vec4(max(result,vec3(0)),1);
}
)POST";
inline const char* PostDown=R"POST(#version 300 es
precision highp float;
out vec4 outColor;

uniform sampler2D sourceTexture;
uniform vec2 sourceInvResolution;
in vec2 uv0;
#ifdef PREMULTIPLY
	uniform sampler2D premultiplyTexture;
#endif
void main()
{
	vec3 e = texture (sourceTexture, uv0).rgb;
	#ifdef BOXFILTER
		vec3 value = e;
		#ifdef PREMULTIPLY
			float premultiply = texture(premultiplyTexture, uv0).{PREMULTIPLY_SRC_CHANNEL};
			value *= vec3(premultiply);
		#endif
	#else
		float x = sourceInvResolution.x;
		float y = sourceInvResolution.y;
		vec3 a = texture(sourceTexture, vec2 (uv0.x - 2.0 * x, uv0.y + 2.0 * y)).rgb;
		vec3 b = texture(sourceTexture, vec2 (uv0.x,		   uv0.y + 2.0 * y)).rgb;
		vec3 c = texture(sourceTexture, vec2 (uv0.x + 2.0 * x, uv0.y + 2.0 * y)).rgb;
		vec3 d = texture(sourceTexture, vec2 (uv0.x - 2.0 * x, uv0.y)).rgb;
		vec3 f = texture(sourceTexture, vec2 (uv0.x + 2.0 * x, uv0.y)).rgb;
		vec3 g = texture(sourceTexture, vec2 (uv0.x - 2.0 * x, uv0.y - 2.0 * y)).rgb;
		vec3 h = texture(sourceTexture, vec2 (uv0.x,		   uv0.y - 2.0 * y)).rgb;
		vec3 i = texture(sourceTexture, vec2 (uv0.x + 2.0 * x, uv0.y - 2.0 * y)).rgb;
		vec3 j = texture(sourceTexture, vec2 (uv0.x - x, uv0.y + y)).rgb;
		vec3 k = texture(sourceTexture, vec2 (uv0.x + x, uv0.y + y)).rgb;
		vec3 l = texture(sourceTexture, vec2 (uv0.x - x, uv0.y - y)).rgb;
		vec3 m = texture(sourceTexture, vec2 (uv0.x + x, uv0.y - y)).rgb;
		vec3 value = e * 0.125;
		value += (a + c + g + i) * 0.03125;
		value += (b + d + f + h) * 0.0625;
		value += (j + k + l + m) * 0.125;
	#endif
	#ifdef REMOVE_INVALID
		value = max(value, vec3(0.0));
	#endif
	outColor = vec4(value, 1.0);
}
)POST";
inline const char* PostUp=R"POST(#version 300 es
precision highp float;
out vec4 outColor;

	uniform sampler2D sourceTexture;
	uniform vec2 sourceInvResolution;
	in vec2 uv0;
	void main()
	{
		float x = sourceInvResolution.x;
		float y = sourceInvResolution.y;
		vec3 a = texture (sourceTexture, vec2 (uv0.x - x, uv0.y + y)).rgb;
		vec3 b = texture (sourceTexture, vec2 (uv0.x,	 uv0.y + y)).rgb;
		vec3 c = texture (sourceTexture, vec2 (uv0.x + x, uv0.y + y)).rgb;
		vec3 d = texture (sourceTexture, vec2 (uv0.x - x, uv0.y)).rgb;
		vec3 e = texture (sourceTexture, vec2 (uv0.x,	 uv0.y)).rgb;
		vec3 f = texture (sourceTexture, vec2 (uv0.x + x, uv0.y)).rgb;
		vec3 g = texture (sourceTexture, vec2 (uv0.x - x, uv0.y - y)).rgb;
		vec3 h = texture (sourceTexture, vec2 (uv0.x,	 uv0.y - y)).rgb;
		vec3 i = texture (sourceTexture, vec2 (uv0.x + x, uv0.y - y)).rgb;
		vec3 value = e * 0.25;
		value += (b + d + f + h) * 0.125;
		value += (a + c + g + i) * 0.0625;
		outColor = vec4(value, 1.0);
	}
)POST";
}
