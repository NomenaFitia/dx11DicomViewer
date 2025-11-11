
// CBuffers existants 

cbuffer CBFrame : register(b0)
{
    row_major float4x4 View;
    row_major float4x4 Proj;
    float3 LightDir;
    float _pad0; // lumi-re direction en WORLD 
};
cbuffer CBObject : register(b1)
{
    row_major float4x4 World;
};
cbuffer CBMaterial : register(b2)
{
    float3 BaseColor;
    float _pad1;
}

// Réglages artistiques (constantes internes, pas de nouveau buffer)

static const float AMBIENT_BASE = 0.04f; // plancher ambiant
static const float3 SKY_COLOR = float3(0.35, 0.45, 0.60);
static const float3 GROUND_COLOR = float3(0.15, 0.12, 0.10);
static const float HEMI_STRENGTH = 0.35f; // poids de l'hémisphère
static const float WRAP_K = 0.4f; // 0=Lambert pur; ~0.3–0.6 = doux
static const float SPEC_POWER = 64.0f; // brillance Blinn
static const float SPEC_STRENGTH = 0.4f; // force spéculaire
static const float RIM_POWER = 3.0f; // 1=large, >3 = plus fin
static const float RIM_STRENGTH = 0.15f; // intensité rim

// I/O

struct VSIn
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
};

struct VSOut
{
    float4 posH : SV_Position; // clip
    float3 Nv : NORMAL; // normale en espace VUE
    float Ny : TEXCOORD0; // composante "up" (pour hémisphère)
    float3 V : TEXCOORD1; // vecteur vers caméra (vue)
};

// VS 

VSOut VSMain(VSIn vin)
{
    VSOut v;

    // world
    float4 wp = mul(float4(vin.pos, 1.0f), World);
    float3 nW = normalize(mul(float4(vin.normal, 0.0f), World).xyz);

    // vue
    float4 vp = mul(wp, View);
    float3 nV = normalize(mul(float4(nW, 0.0f), View).xyz);

    v.posH = mul(vp, Proj);
    v.Nv = nV;
    v.Ny = nV.y; // approx "up" (si caméra pas trop inclinée)
    v.V = -vp.xyz; // caméra à l'origine en espace vue

    return v;
}

// Helpers

float DiffuseWrap(float ndotl, float k)
{
    // "wrapped diffuse" (Valve/half-Lambert like)
    // k in [0..1] – augmente la lumière sur les faces tournées
    return saturate((ndotl + k) / (1.0f + k));
}

// PS : wrap diffuse + hémisphère + spec + rim 

float4 PSMain(VSOut pin) : SV_Target
{
    float3 N = normalize(pin.Nv);
    float3 V = normalize(pin.V);

    // Lumière directionnelle -> espace vue (LightDir est WORLD)
    float3 Lw = -normalize(LightDir); // vecteur pointant vers la lumière
    float3 L = normalize(mul(float4(Lw, 0.0f), View).xyz);

    // Lambert "wrap" : plus de lumière de dos, mais garde le modelé
    float ndotl = dot(N, L);
    float diffWrapped = DiffuseWrap(ndotl, WRAP_K);

    // Ambiant hémisphérique : interpole ciel/sol selon Ny
    float hemiT = saturate(pin.Ny * 0.5f + 0.5f); // [-1..1] -> [0..1]
    float3 hemi = lerp(GROUND_COLOR, SKY_COLOR, hemiT) * HEMI_STRENGTH;

    // Spéculaire Blinn-Phong (optionnel, doux)
    float3 H = normalize(L + V);
    float spec = pow(saturate(dot(N, H)), SPEC_POWER) * SPEC_STRENGTH;

    // Rim light doux sur les silhouettes (optionnel)
    float rim = pow(saturate(1.0f - dot(N, V)), RIM_POWER) * RIM_STRENGTH;

    // Assemblage
    float3 color =
        BaseColor * (AMBIENT_BASE + hemi) + // ambiant (plancher + hémisphère)
        BaseColor * diffWrapped + // diffuse adouci
        spec.xxx + // spec blanc (faute de SpecColor)
        rim.xxx; // liseré subtil

    return float4(saturate(color), 1.0f);
}
