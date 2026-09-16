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
uniform highp usampler2D shLabels;
uniform highp usampler2D shCentroids;
uniform highp sampler2D shBooks;
uniform bool shFlip;
vec3 directionalColor(vec3 position,mat4 cameraView){
 vec3 sh[15];vec3 dc;
 if(shCompressed){
  uvec2 code=texelFetch(shLabels,ivec2(int(splatIndex%4096u),int(splatIndex/4096u)),0).rg;
  int n=shSourceBands==1?3:shSourceBands==2?8:15;
  int x=int(code.x%64u)*n,y=int(code.x/64u);
  for(int c=0;c<3;c++)dc[c]=0.5+0.28209479177387814*texelFetch(shBooks,ivec2(int((code.y>>uint(c*8))&255u),0),0).r;
  for(int i=0;i<15;i++){
   sh[i]=vec3(0.0);
   if(i<n&&!((shBands==1&&i>=3)||(shBands==2&&i>=8))){uvec3 v=texelFetch(shCentroids,ivec2(x+i,y),0).rgb;for(int c=0;c<3;c++)sh[i][c]=texelFetch(shBooks,ivec2(int(v[c]),0),0).g;}
  }
 }else{
 vec4 packedSH[12];
 for(int i=0;i<12;i++){uint a=splatIndex*12u+uint(i);packedSH[i]=texelFetch(shData,ivec2(int(a%4096u),int(a/4096u)),0);}
 for(int i=0;i<15;i++)for(int c=0;c<3;c++){int p=3+i*3+c;sh[i][c]=((shBands==1&&i>=3)||(shBands==2&&i>=8))?0.0:packedSH[p/4][p%4];}
 dc=packedSH[0].xyz;
 }
 vec3 eye=-transpose(mat3(cameraView))*cameraView[3].xyz;
 vec3 delta=position-eye;vec3 dir=delta/max(length(delta),0.0000001);
 if(shFlip)dir.xy=-dir.xy;
 return max(vec3(0.0),dc+evalSH(sh,dir));
}
)SH";
}
