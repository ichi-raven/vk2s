#ifndef PBRTBSDFS_GLSL_
#define PBRTBSDFS_GLSL_

precision highp float;
precision highp int;

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_control_flow_attributes : enable

#include "types.glsl"
#include "constants.glsl"
#include "randoms.glsl"
#include "BSDFs.glsl"

// Maths-------------------------------------
void basis(in vec3 n, out vec3 b1, out vec3 b2) 
{
    if(n.z < 0.)
    {
        const float a = 1.0 / (1.0 - n.z);
        const float b = n.x * n.y * a;
        b1 = vec3(1.0 - n.x * n.x * a, -b, n.x);
        b2 = vec3(b, n.y * n.y * a - 1.0, -n.y);
    }
    else
    {
        const float a = 1.0 / (1.0 + n.z);
        const float b = -n.x * n.y * a;
        b1 = vec3(1.0 - n.x * n.x * a, b, -n.x);
        b2 = vec3(b, 1.0 - n.y * n.y * a, -n.y);
    }
}

vec3 toWorld(vec3 x, vec3 y, vec3 z, vec3 v)
{
    return v.x * x + v.y * y + v.z * z;
}

vec3 toLocal(vec3 x, vec3 y, vec3 z, vec3 v)
{
    return vec3(dot(v, x), dot(v, y), dot(v, z));
}

float sqr(const float x)
{
    return x * x;
}

vec2 mulComplex(const vec2 a, const vec2 b)
{
    return vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

vec2 sqrComplex(const vec2 c)
{
    return vec2(c.x * c.x - c.y * c.y, 2. * c.x * c.y);
}

vec2 sqrtComplex(const vec2 a) 
{
    float r = length(a);
    float rpart = sqrt(0.5 * (r + a.x));
    float ipart = sqrt(0.5 * (r - a.x));
    if (a.y < 0.0)
    { 
        ipart = -ipart;
    }
    return vec2(rpart,ipart);
}

vec2 bar(const vec2 c)
{
    return vec2(c.x, -c.y);
}

float sgn(const float v)
{
    if (v == 0.0)
    {
        return 0.0;
    }

    return v >= 0. ? 1.0 : -1.0;
}

float cosTheta(const vec3 v)
{
    return v.z;
}

float cos2Theta(const vec3 v)
{
    const float c = cosTheta(v);
    return c * c;
}

float sinTheta(const vec3 v)
{
    return sqrt(1.0 - cos2Theta(v));
}

float tanTheta(const vec3 v)
{
    float c = cosTheta(v);
    return sqrt(1. - c * c) / c;
}

float tan2Theta(const vec3 v)
{
    const float c2 = cos2Theta(v);
    return 1. / c2 - 1.;
}

float cosPhi(const vec3 v)
{
    float s = sinTheta(v);
    return (s == 0) ? 1.0 : clamp(v.x / s, -1.0, 1.0);
}

float cos2Phi(const vec3 v)
{
    const float c = cosPhi(v);
    return c * c;
}

float sin2Phi(const vec3 v)
{
    const float c2 = cos2Phi(v);
    return 1 - c2;
}

float sinPhi(const vec3 v)
{
    return sqrt(sin2Phi(v));
}

bool refract(const vec3 wi, vec3 normal, float eta, out float etap, out vec3 wt)
{
    float ct_i = dot(normal, wi);
    if (ct_i < 0.)
    {
        eta = 1. / eta;
        ct_i = -ct_i;
        normal = -normal;
    }

    const float s2t_i = max(0., 1. - sqr(ct_i));
    const float s2t_t = s2t_i / sqr(eta);
    if (s2t_t >= 1.)
    {
        return false;
    }

    const float ct_t = sqrt(1. - s2t_t);
    wt = -wi / eta + (ct_i / eta - ct_t) * normal;
    etap = eta;

    return true;
}

// Fresnel---
float frDielectric(float ct_i, float eta)
{
    ct_i = clamp(ct_i, -1., 1.);
    if (ct_i < 0.0)
    {
        eta = 1. / eta;
        ct_i = -ct_i;
    }

    const float s2t_i = 1. - sqr(ct_i);
    const float s2t_t = s2t_i / sqr(eta);

    if (s2t_t >= 1.)
    {
        return 1.;
    }

    const float ct_t = sqrt(1. - s2t_t);

    const float rParl = (eta * ct_i - ct_t) / (eta * ct_i + ct_t);
    const float rPerp = (ct_i - eta * ct_t) / (ct_i + eta * ct_t);

    return 0.5 * (sqr(rParl) + sqr(rPerp));
}

// in here, treat complex as vec2 (x = Re, y = Im)
float frComplex(float ct_i, const vec2 eta)
{
    ct_i = clamp(ct_i, 0., 1.);

    const float s2t_i = 1. - sqr(ct_i);
    
    const float c = sqr(eta.x) - sqr(eta.y);
    const float d = 2. * eta.x * eta.y;

    const vec2 s2t_t = s2t_i * vec2(c, -d) / (sqr(c) + sqr(d));
    const vec2 ct_t = sqrtComplex(vec2(1., 0.) - s2t_t);

    const vec2 rParl = (eta * ct_i - ct_t) / (eta * ct_i + ct_t);
    const vec2 rPerp = (ct_i - mulComplex(eta, ct_t)) / (ct_i + mulComplex(eta, ct_t));

    return 0.5 * (dot(rParl, rParl) + dot(rPerp, rPerp));
}

// GGX---
bool isEffectivelySmooth(const float ax, const float ay)
{
    return max(ax, ay) < EPS;
}

float lambda(const vec3 w, const float ax, const float ay)
{
    const float t2t = tan2Theta(w);
    if (t2t >= INFTY || t2t <= -INFTY)
    {
        return 0.;
    }

    const float a2 = sqr(ax * cosPhi(w)) + sqr(ay * sinPhi(w));

    return 0.5 * (sqrt(1. + a2 * t2t) - 1.);
}

float G1(const vec3 w, const float ax, const float ay)
{
    return 1. / (1. + lambda(w, ax, ay));
}

float G(const vec3 wo, const vec3 wi, const float ax, const float ay)
{
    return 1. / (1. + lambda(wo, ax, ay) + lambda(wi, ax, ay));
}

float D(const vec3 wm, const float ax, const float ay)
{
    const float t2t = tan2Theta(wm);
    if (t2t >= INFTY || t2t <= -INFTY)
    {
        return 0.;
    }

    const float c4t = sqr(cos2Theta(wm));

    const float e = t2t * (sqr(cosPhi(wm) / ax) + sqr(sinPhi(wm) / ay));

    return 1. / (M_PI * ax * ay * c4t * sqr(1. + e));
}

float D(const vec3 w, const vec3 wm, const float ax, const float ay)
{
    return G1(w, ax, ay) / abs(cosTheta(w)) * D(wm, ax, ay) * abs(dot(w, wm));
}

float pdfGGX(const vec3 w, const vec3 wm, const float ax, const float ay)
{
    return D(w, wm, ax, ay);
}

vec3 sampleWmGGX(const vec3 w, const vec2 u, const float ax, const float ay)
{
    // transform w to hemispherical configuration
    vec3 wh = normalize(vec3(ax * w.x, ay * w.y, w.z));
    if (wh.z < 0.0)
    {
        wh = -wh;
    }

    // find ONB for visible normal sampling
    vec3 T1, T2;
    basis(wh, T1, T2);
    // const vec3 T1 = wh.z < (1. - EPS) ? normalize(cross(vec3(0., 0., 1.), wh)) : vec3(1., 0., 0.);
    // const vec3 T2 = cross(wh, T1);

    //generate uniformly distributed points on the unit disk
    vec2 p = randomUniformDiskPolar(u);

    // warp hemispherical projection for visible normal sampling
    const float h = sqrt(1. - sqr(p.x));
    p.y = mix(h, p.y, (1. + wh.z) / 2.);

    // reproject to hemisphere and transform normal to ellipsoid configuration
    const float pz = sqrt(max(0., 1. - dot(p, p)));
    const vec3 nh = toWorld(T1, T2, wh, vec3(p.x, p.y, pz));
    return normalize(vec3(ax * nh.x, ay * nh.y, max(EPS, nh.z)));
}

// BSDFs-----------------------------------

// Diffuse---
vec3 evalDiffuse(const vec3 albedo, const vec3 wo, const vec3 wi)
{
    if (wo.z * wi.z <= 0.) // not in same hemisphere
    {
        return vec3(0.0);
    }

    return albedo * M_INVPI;
}

BSDFSample sampleDiffuse(const vec3 albedo, const vec3 wo, inout uint prngState)
{
    BSDFSample ret;
    ret.flags = BSDF_FLAGS_DIFFUSE_REFLECTION;
    
    ret.f = albedo.xyz * M_INVPI;
    ret.wi = randomCosDirection(prngState);
    if (wo.z < 0.0)
    {
        ret.wi.z *= -1.0;
    }
    ret.pdf = M_INVPI * abs(ret.wi.z);

    return ret;
}

float pdfDiffuse(const vec3 wo, const vec3 wi)
{
    if (wo.z * wi.z <= 0.) // not in same hemisphere
    {
        return 0.;
    }

    return M_INVPI * abs(wi.z);
}

// Conductor---

vec3 evalConductor(const vec3 albedo, const float ax, const float ay, const float eta, const float k, const vec3 wo, const vec3 wi)
{
    // not in same hemisphere or specular reflection
    if (wo.z * wi.z <= 0. || isEffectivelySmooth(ax, ay)) 
    {
        return vec3(0.);
    }

    const float ct_o = abs(cosTheta(wo)), ct_i = abs(cosTheta(wi));
    if (ct_i == 0. || ct_o == 0.)
    {
        return vec3(0.);
    }

    vec3 wm = wi + wo;
    if (dot(wm, wm) == 0.) // invalid microfacet normal
    {
        return vec3(0.);
    }
    wm = normalize(wm);

    const float F = frComplex(abs(dot(wo, wm)), vec2(eta, k));

    return vec3(D(wm, ax, ay) * F * G(wo, wi, ax, ay) / (4. * ct_i * ct_o));
}

BSDFSample sampleConductor(const vec3 albedo, const float ax, const float ay, const float eta, const float k, const vec3 wo, const vec2 u)
{
    BSDFSample ret;
    ret.eta = 1.;
    ret.f = vec3(0.0);
    ret.pdf = 1.0;

    if (isEffectivelySmooth(ax, ay))
    {
        ret.flags = BSDF_FLAGS_SPECULAR_REFLECTION;
        ret.pdf = 1.0;// delta
        const vec3 wi = vec3(-wo.x, -wo.y, wo.z);
        const float absctwi = abs(cosTheta(wi));
        ret.f = vec3(frComplex(absctwi, vec2(eta, k)) / absctwi);
        ret.wi = wi;
        return ret;
    }

    // glossy
    ret.flags = BSDF_FLAGS_GLOSSY_REFLECTION;
    const vec3 wm = sampleWmGGX(wo, u, ax, ay);
    const vec3 wi = reflect(-wo, wm);
    if (wo.z * wi.z <= 0.) // not in same hemisphere
    {
        ret.flags = BSDF_FLAGS_FAILED;
        return ret;
    }

    const float ct_o = abs(cosTheta(wo));
    const float ct_i = abs(cosTheta(wi));
    // Fresnel term
    const float F = frComplex(abs(dot(wo, wm)), vec2(eta, k));
    ret.pdf = pdfGGX(wo, wm, ax, ay) / (4. * abs(dot(wo, wm)));
    ret.f = vec3(D(wm, ax, ay) * F * G(wo, wi, ax, ay) / (4. * ct_i * ct_o));
    ret.wi = wi;

    return ret;
}

float pdfConductor(const vec3 wo, const vec3 wi, const float ax, const float ay)
{
    if (wo.z * wi.z <= 0.) // not in same hemisphere
    {
        return 0.;
    }

    vec3 wm = wo + wi;
    if (dot(wm, wm) == 0.)
    {
        return 0.;
    }
    wm = faceforward(wm, wm, vec3(0., 0., 1.));

    return pdfGGX(wo, wm, ax, ay) / (4. * abs(dot(wo, wm)));
}

// Dielectric---

vec3 evalDielectric(const float ax, const float ay, const float eta, const vec3 wo, const vec3 wi)
{
    if (eta == 1.0 || isEffectivelySmooth(ax, ay))
    {
        return vec3(0.);
    }

    const float ct_o = cosTheta(wo);
    const float ct_i = cosTheta(wi);

    bool refl = ct_i * ct_o > 0.;
    float etap = 1.;

    if (!refl)
    {
        etap = ct_o > 0. ? eta : (1. / eta);
    }

    vec3 wm = wi * etap + wo;
    if (ct_i == 0. || ct_o == 0. || dot(wm, wm) == 0.)
    {
        return vec3(0.);
    }

    wm = faceforward(wm, wm, vec3(0., 0., 1.));

    // discard backfacing microfacets
    if (dot(wm, wi) * ct_i < 0. || dot(wm, wo) * ct_o < 0.)
    {
        return vec3(0.);
    }

    const float F = frDielectric(dot(wo, wm), eta);

    if (refl)
    {
        return vec3(D(wm, ax, ay) * F * G(wo, wi, ax, ay) / (4. * ct_i * ct_o));
    }
    else
    {
        const float denom = sqr(dot(wi, wm) + dot(wo, wm) / etap) * ct_i * ct_o;
        const float dwm_dwi = abs(dot(wi, wm)) / denom;

        return vec3((1. - F) * D(wm, ax, ay) * G(wo, wi, ax, ay)* abs(dot(wi, wm)) * abs(dot(wo, wm)) / denom);
    }
}

BSDFSample sampleDielectric(const float ax, const float ay, const float eta, const vec3 wo, const float uc, const vec2 u)
{
    BSDFSample ret;
    ret.flags |= isEffectivelySmooth(ax, ay) ? BSDF_FLAGS_SPECULAR : BSDF_FLAGS_GLOSSY;
    ret.eta = eta;
    ret.f = vec3(0.0);
    ret.pdf = 1.0;

    if (eta == 1.0 || isEffectivelySmooth(ax, ay)) // specular
    {
        const float pR = frDielectric(cosTheta(wo), eta);
        const float pT = 1. - pR;

        if (uc < pR / (pR + pT))
        {
            // sample perfect specular BRDF
            ret.flags = BSDF_FLAGS_SPECULAR_REFLECTION;
            ret.wi = vec3(-wo.x, -wo.y, wo.z);
            ret.f = vec3(pR / abs(cosTheta(ret.wi)));
            ret.pdf = pR / (pR + pT);
            return ret;
        }
        else
        {
            // sample perfect specular BTDF
            ret.flags = BSDF_FLAGS_SPECULAR_TRANSMISSION;
            vec3 wi;
            float etap;
            bool valid = refract(wo, vec3(0., 0., 1.), eta, etap, wi);
            if (!valid)
            {
                ret.flags = BSDF_FLAGS_FAILED;
                return ret;
            }

            ret.f = vec3(pT / abs(cosTheta(wi)));
            ret.pdf = pT / (pR + pT);
            ret.f /= sqr(etap); // if radiance
            ret.wi = wi;
            return ret;
        }
    }
    else // glossy
    {
        const vec3 wm = sampleWmGGX(wo, u, ax, ay);
        const float pR = frDielectric(dot(wo, wm), eta);
        const float pT = 1. - pR;

        if (uc < pR / (pR + pT)) // reflected
        {
            ret.flags = BSDF_FLAGS_GLOSSY_REFLECTION;
            const vec3 wi = reflect(-wo, wm);
            if (wo.z * wi.z <= 0.) // not in same hemisphere
            {
                ret.flags = BSDF_FLAGS_FAILED;
                return ret;
            }

            const float ct_o = cosTheta(wo);
            const float ct_i = cosTheta(wi);
            ret.pdf = pdfGGX(wo, wm, ax, ay) / (4. * abs(dot(wo, wm))) * pR / (pR + pT);
            ret.f = vec3(D(wm, ax, ay) * G(wo, wi, ax, ay) * pR / (4. * ct_i * ct_o));
            ret.wi = wi;

            return ret;
        }
        else // transmitted
        {
            ret.flags = BSDF_FLAGS_GLOSSY_TRANSMISSION;
            vec3 wi;
            float etap;
            bool tir = !refract(wo, wm, eta, etap, wi);

            if (wo.z * wi.z > 0. || wi.z == 0. || tir) // in same hemisphere or failed to refract
            {
                ret.flags = BSDF_FLAGS_FAILED;
                return ret;
            }

            const float ct_o = cosTheta(wo);
            const float ct_i = cosTheta(wi);
            const float denom = sqr(dot(wi, wm) + dot(wo, wm) / etap);
            const float dwm_dwi = abs(dot(wi, wm)) / denom;
            ret.pdf = pdfGGX(wo, wm, ax, ay) * dwm_dwi * pT / (pR + pT);
            ret.f = vec3(pT * D(wm, ax, ay) * G(wo, wi, ax, ay) * abs(dot(wi, wm) * dot(wo, wm) / (ct_i * ct_o * denom)));
            ret.wi = wi;

            ret.f /= sqr(etap); // if radiance

            return ret;
        }
    }

    // invalid
    return ret;
}

float pdfDielectric(const vec3 wo, const vec3 wi, const float ax, const float ay, const float eta)
{
    if (eta == 1.0 || isEffectivelySmooth(ax, ay))
    {
        return 0.;
    }
    
    const float ct_o = cosTheta(wo);
    const float ct_i = cosTheta(wi);

    bool refl = ct_i * ct_o > 0.;
    float etap = 1.;

    if (!refl)
    {
        etap = ct_o > 0. ? eta : (1. / eta);
    }

    // compute generalized half vector wm
    vec3 wm = wi * etap + wo;
    if (ct_i == 0. || ct_o == 0. || dot(wm, wm) == 0.)
    {
        return 0.;
    }

    wm = faceforward(wm, wm, vec3(0., 0., 1.));

    // discard backfacing microfacets
    if (dot(wm, wi) * ct_i < 0. || dot(wm, wo) * ct_o < 0.)
    {
        return 0.;
    }

    const float pR = frDielectric(dot(wo, wm), eta);
    const float pT = 1. - pR;
    
    float pdf = EPS;
    if (refl)
    {
        pdf = pdfGGX(wo, wm, ax, ay) / (4. * abs(dot(wo, wm))) * pR / (pR + pT);
    }
    else
    {
        const float denom = sqr(dot(wi, wm) + dot(wo, wm) / etap);
        const float dwm_dwi = abs(dot(wi, wm)) / denom;
        pdf = pdfGGX(wo, wm, ax, ay) * dwm_dwi * pT / (pR + pT);
    }

    return pdf;
}

// interface-----------------------------------

vec3 evalPBRTBSDF(const Material mat, vec3 wo, vec3 wi, const vec3 normal)
{
    // calc tangent space
    vec3 T, B;
    basis(normal, T, B);

    wo = normalize(toLocal(T, B, normal, wo));
    wi = normalize(toLocal(T, B, normal, wi));

    if (wo.z == 0.) // completely horizontal incidence 
    {
        return vec3(0.);
    }

    const float ax = mat.alpha;
    const float ay = mat.alpha;

    // currently unused complex eta
    const float eta = mat.IOR;
    const float k = 1.0;

    switch(mat.matType)
    {
        case MAT_LAMBERT:
            return evalDiffuse(mat.albedo.xyz, wo, wi);
        break;
        case MAT_CONDUCTOR:
            return evalConductor(mat.albedo.xyz, ax, ay, eta, k, wo, wi); 
        break;
        case MAT_DIELECTRIC:
            return evalDielectric(ax, ay, eta, wo, wi);
        break;
        default:
            // ERROR
        break;
    }

    // invalid
    return vec3(0.);
}

BSDFSample samplePBRTBSDF(const Material mat, vec3 wo, const vec3 normal, inout uint prngState)
{
    BSDFSample ret;
    ret.f = mat.albedo.xyz;
    ret.pdf = 1.0;
    ret.flags = 0;

    const vec2 u = vec2(stepAndOutputRNGFloat(prngState), stepAndOutputRNGFloat(prngState));
    const float uc = stepAndOutputRNGFloat(prngState);

    const float ax = mat.alpha;
    const float ay = mat.alpha;

    const float eta = mat.IOR;
    const float k = 1.0;

    // calc tangent space
    vec3 T, B;
    basis(normal, T, B);

    wo = normalize(toLocal(T, B, normal, wo));

    if (wo.z == 0.) // completely horizontal incidence 
    {
        ret.flags = BSDF_FLAGS_FAILED;
        return ret;
    }

    switch(mat.matType)
    {
        case MAT_LAMBERT:
            ret = sampleDiffuse(mat.albedo.xyz, wo, prngState);
        break;
        case MAT_CONDUCTOR:
            ret = sampleConductor(mat.albedo.xyz, ax, ay, eta, k, wo, u); 
        break;
        case MAT_DIELECTRIC:
            ret = sampleDielectric(ax, ay, eta, wo, uc, u);
        break;
        default:
            // ERROR
            ret.flags = BSDF_FLAGS_FAILED;
        break;
    }

    ret.wi = toWorld(T, B, normal, ret.wi);

    return ret;
}

float pdfPBRTBSDF(const Material mat, vec3 wo, vec3 wi, vec3 normal)
{
    // calc tangent space
    vec3 T, B;
    basis(normal, T, B);

    wo = normalize(toLocal(T, B, normal, wo));
    wi = normalize(toLocal(T, B, normal, wi));

    if (wo.z == 0.) // completely horizontal incidence 
    {
        return 0.;
    }

    const float ax = mat.alpha;
    const float ay = mat.alpha;

    const float eta = mat.IOR;

    switch(mat.matType)
    {
        case MAT_LAMBERT:
            return pdfDiffuse(wo, wi);
        break;
        case MAT_CONDUCTOR:
            return pdfConductor(wo, wi, ax, ay);
        break;
        case MAT_DIELECTRIC:
            return pdfDielectric(wo, wi, ax, ay, eta);
        break;
        default:
        // ERROR
        break;
    }

    // invalid
    return 0.;
}

#endif