#pragma once
namespace splat {
// PlayCanvas 2.21.3 gsplatEvalSH, MIT; see THIRD_PARTY_NOTICES.md.
inline const char* ShShader=R"SH(
#define SH_BANDS 3

	#if SH_BANDS == 1
		#define SH_COEFFS 3
	#elif SH_BANDS == 2
		#define SH_COEFFS 8
	#elif SH_BANDS == 3
		#define SH_COEFFS 15
	#else
		#define SH_COEFFS 0
	#endif
	#if SH_BANDS > 0
	const float SH_C1 = 0.4886025119029199f;
	#if SH_BANDS > 1
		const float SH_C2_0 = 1.0925484305920792f;
		const float SH_C2_1 = -1.0925484305920792f;
		const float SH_C2_2 = 0.31539156525252005f;
		const float SH_C2_3 = -1.0925484305920792f;
		const float SH_C2_4 = 0.5462742152960396f;
	#endif
	#if SH_BANDS > 2
		const float SH_C3_0 = -0.5900435899266435f;
		const float SH_C3_1 = 2.890611442640554f;
		const float SH_C3_2 = -0.4570457994644658f;
		const float SH_C3_3 = 0.3731763325901154f;
		const float SH_C3_4 = -0.4570457994644658f;
		const float SH_C3_5 = 1.445305721320277f;
		const float SH_C3_6 = -0.5900435899266435f;
	#endif
	vec3 evalSH(in vec3 sh[SH_COEFFS], in vec3 dir) {
		float x = dir.x;
		float y = dir.y;
		float z = dir.z;
		vec3 result = SH_C1 * (-sh[0] * y + sh[1] * z - sh[2] * x);
		#if SH_BANDS > 1
			float xx = x * x;
			float yy = y * y;
			float zz = z * z;
			float xy = x * y;
			float yz = y * z;
			float xz = x * z;
			result +=
				sh[3] * (SH_C2_0 * xy) +
				sh[4] * (SH_C2_1 * yz) +
				sh[5] * (SH_C2_2 * (2.0 * zz - xx - yy)) +
				sh[6] * (SH_C2_3 * xz) +
				sh[7] * (SH_C2_4 * (xx - yy));
		#endif
		#if SH_BANDS > 2
			result +=
				sh[8]  * (SH_C3_0 * y * (3.0 * xx - yy)) +
				sh[9]  * (SH_C3_1 * xy * z) +
				sh[10] * (SH_C3_2 * y * (4.0 * zz - xx - yy)) +
				sh[11] * (SH_C3_3 * z * (2.0 * zz - 3.0 * xx - 3.0 * yy)) +
				sh[12] * (SH_C3_4 * x * (4.0 * zz - xx - yy)) +
				sh[13] * (SH_C3_5 * z * (xx - yy)) +
				sh[14] * (SH_C3_6 * x * (xx - 3.0 * yy));
		#endif
		return result;
	}
	#endif

uniform highp sampler2D shData;
uniform int shBands;
uniform int shSourceBands;
uniform bool shCompressed;
uniform bool shPaged;
uniform highp usampler2D shLabels;
uniform highp usampler2D shCentroids;
uniform highp sampler2D shBooks;
uniform highp sampler2D shDecoded;
uniform bool shFlip;
// Keep coefficients out of dynamically indexed local arrays. Mobile drivers can
// spill those arrays to memory, multiplying the cost for every Gaussian.
vec3 shCoefficient(int i,uint row,uint label,int n){
#ifdef SH_SINGLE_SOG
 return texelFetch(shDecoded,ivec2(int(label%64u)*n+i,int(label/64u)),0).rgb;
#else
 if(shPaged){
  uint logical=label*uint(n)+uint(i),page=texelFetch(shLabels,ivec2(int(logical/16384u),int(row)),0).r;
  uint address=page*16384u+logical%16384u;
  uvec3 v=texelFetch(shCentroids,ivec2(int(address%4096u),int(address/4096u)),0).rgb;
  return vec3(texelFetch(codebooks,ivec2(int(v.x),int(row)),0).z,
              texelFetch(codebooks,ivec2(int(v.y),int(row)),0).z,
              texelFetch(codebooks,ivec2(int(v.z),int(row)),0).z);
 }
 if(shCompressed)return texelFetch(shDecoded,ivec2(int(label%64u)*n+i,int(label/64u)),0).rgb;
 int p=3+i*3;uint a=splatIndex*12u+uint(p/4);
 vec4 lo=texelFetch(shData,ivec2(int(a%4096u),int(a/4096u)),0);
 vec4 hi=texelFetch(shData,ivec2(int((a+1u)%4096u),int((a+1u)/4096u)),0);
 if(p%4==0)return lo.xyz;if(p%4==1)return lo.yzw;if(p%4==2)return vec3(lo.zw,hi.x);return vec3(lo.w,hi.xy);
#endif
}
vec3 directionalColor(vec3 position,mat4 cameraView,vec3 baseColor){
 uint row=0u,label=0u;int bands=min(shBands,shSourceBands),n=0;vec3 dc;
#ifdef SH_SINGLE_SOG
  uvec2 code=texelFetch(shLabels,ivec2(int(splatIndex%4096u),int(splatIndex/4096u)),0).rg;
  label=code.x;n=shSourceBands==1?3:shSourceBands==2?8:15;
  dc=0.5+0.28209479177387814*vec3(texelFetch(shBooks,ivec2(int(code.y&255u),0),0).r,
          texelFetch(shBooks,ivec2(int((code.y>>8u)&255u),0),0).r,
          texelFetch(shBooks,ivec2(int((code.y>>16u)&255u),0),0).r);

#else
 if(shPaged){
  uvec4 code=texelFetch(encodedCodes,ivec2(int(splatIndex%4096u),int(splatIndex/4096u)),0);
  row=code.w&2047u;label=(code.w>>11u)&65535u;int source=int((code.w>>27u)&3u);
  bands=min(shBands,source);if(bands==0)return baseColor;n=source==1?3:source==2?8:15;
  dc=vec3(texelFetch(codebooks,ivec2(int(code.z&255u),int(row)),0).w,
          texelFetch(codebooks,ivec2(int((code.z>>8u)&255u),int(row)),0).w,
          texelFetch(codebooks,ivec2(int((code.z>>16u)&255u),int(row)),0).w);
 }else if(shCompressed){
  uvec2 code=texelFetch(shLabels,ivec2(int(splatIndex%4096u),int(splatIndex/4096u)),0).rg;
  label=code.x;n=shSourceBands==1?3:shSourceBands==2?8:15;
  dc=0.5+0.28209479177387814*vec3(texelFetch(shBooks,ivec2(int(code.y&255u),0),0).r,
          texelFetch(shBooks,ivec2(int((code.y>>8u)&255u),0),0).r,
          texelFetch(shBooks,ivec2(int((code.y>>16u)&255u),0),0).r);
 }else{uint a=splatIndex*12u;dc=texelFetch(shData,ivec2(int(a%4096u),int(a/4096u)),0).xyz;}
#endif
 vec3 eye=-transpose(mat3(cameraView))*cameraView[3].xyz;
 vec3 delta=position-eye;vec3 dir=delta/max(length(delta),0.0000001);
 if(shFlip)dir.xy=-dir.xy;
 float x=dir.x,y=dir.y,z=dir.z,xx=x*x,yy=y*y,zz=z*z;
 vec3 result=dc;
 if(bands>=1){
  result+=shCoefficient(0,row,label,n)*(-SH_C1*y);
  result+=shCoefficient(1,row,label,n)*(SH_C1*z);
  result+=shCoefficient(2,row,label,n)*(-SH_C1*x);
 }
 if(bands>=2){
  result+=shCoefficient(3,row,label,n)*(SH_C2_0*x*y);
  result+=shCoefficient(4,row,label,n)*(SH_C2_1*y*z);
  result+=shCoefficient(5,row,label,n)*(SH_C2_2*(2.0*zz-xx-yy));
  result+=shCoefficient(6,row,label,n)*(SH_C2_3*x*z);
  result+=shCoefficient(7,row,label,n)*(SH_C2_4*(xx-yy));
 }
 if(bands>=3){
  result+=shCoefficient(8,row,label,n)*(SH_C3_0*y*(3.0*xx-yy));
  result+=shCoefficient(9,row,label,n)*(SH_C3_1*x*y*z);
  result+=shCoefficient(10,row,label,n)*(SH_C3_2*y*(4.0*zz-xx-yy));
  result+=shCoefficient(11,row,label,n)*(SH_C3_3*z*(2.0*zz-3.0*xx-3.0*yy));
  result+=shCoefficient(12,row,label,n)*(SH_C3_4*x*(4.0*zz-xx-yy));
  result+=shCoefficient(13,row,label,n)*(SH_C3_5*z*(xx-yy));
  result+=shCoefficient(14,row,label,n)*(SH_C3_6*x*(xx-3.0*yy));
 }
 return max(vec3(0.0),result);
}
)SH";
}
