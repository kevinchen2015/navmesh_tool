

Shader "unlit/vertex_color" {
	Properties{
		_TintColor("Tint Color", Color) = (0.5,0.5,0.5,0.5)
	
	}

	Category{
		Tags{ "Queue" = "Transparent"  "RenderType" = "Transparent"  }
		Blend SrcAlpha OneMinusSrcAlpha
		Cull Off Lighting Off ZWrite Off
		Fog{Mode Off}

		SubShader{
		Pass{

		CGPROGRAM
		#pragma vertex vert
		#pragma fragment frag

		#include "UnityCG.cginc"

		sampler2D _MainTex;
		fixed4 _TintColor;

		struct appdata_t {
			float4 vertex : POSITION;
			fixed4 color : COLOR;
			float2 texcoord : TEXCOORD0;
		};

		struct v2f {
			float4 vertex : SV_POSITION;
			fixed4 color : COLOR;
			float2 texcoord : TEXCOORD0;
		};

		v2f vert(appdata_t v)
		{
			v2f o;
			o.vertex = UnityObjectToClipPos(v.vertex);
			o.color = v.color;
			o.texcoord = v.texcoord;
			return o;
		}
		
		fixed4 frag(v2f i) : SV_Target
		{
			fixed4 col = 2.0f * i.color * _TintColor;
			return col;
		}
		ENDCG
		}
	}
}
}