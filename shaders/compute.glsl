#version 450 core

// Constants
// =========
#define Epsilon 0.0001
#define PI 3.1415926535897932384626433832795
#define MAX_BOUNCE 3
#define MAX_DEPTH 4096
#define MAY_RAYTRACE_DEPTH 4096
#define DIFFUSION_PROB 0.5

// Types
// =====

struct AABB {
  vec3 min;
  vec3 size;
};

struct Material {
  uvec3 rgb;
  uint water;
};

// Inputs
// ======
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(rgba32f, binding = 0) uniform image2D imgOutput;
layout(std430, binding = 1) buffer svdag { int svdagData[]; };
layout(std430, binding = 2) buffer svdagMaterial {
    Material materials[];
};
// output for length
layout(std430, binding = 3) writeonly buffer AFBuffer {
	float AutoFocusLength;
};

uniform int RootSize;
uniform vec3 CameraPos, CameraFront, CameraUp;
uniform vec3 RandomSeed;
uniform int CurrentFrameCount;
uniform bool DepthOfField;
uniform float FocalLength;
uniform float LenRadius;

uniform vec3 SunDir = normalize(vec3(-0.5, 0.75, 0.8));
uniform vec3 SunColor = vec3(1, 1, 1);
uniform vec3 SkyColor = vec3(.53,.81,.92);


// Random
// ======
vec3 RandomSeedCurrent = RandomSeed + vec3(gl_GlobalInvocationID.xy, 1.0);

// Returns a float in range [0, 1).
float constructFloat(uint m) {
  const uint IEEEMantissa = 0x007FFFFFu;
  const uint IEEEOne = 0x3F800000u;
  m = (m & IEEEMantissa) | IEEEOne;
  return uintBitsToFloat(m) - 1.0;
}
uint hash(uint x) { x += x << 10u; x ^= x >> 6u; x += x << 3u; x ^= x >> 11u; x += x << 15u; return x; }
uint hash(uvec2 v) { return hash(v.x ^ hash(v.y)); }
uint hash(uvec3 v) { return hash(v.x ^ hash(v.yz)); }
uint hash(uvec4 v) { return hash(v.x ^ hash(v.yzw)); }
float rand_(vec3 v) { return constructFloat(hash(floatBitsToUint(vec4(v, RandomSeed)))); }
float rand(){
    float ret = rand_(RandomSeedCurrent);
    RandomSeedCurrent += vec3(gl_GlobalInvocationID.xy, ret);
    return ret;
}
// generates a random vec3
vec3 randVec3() {
    float phi = rand() * PI;
    float theta = rand() * 2 * PI;
    return vec3(sin(phi), cos(phi) * sin(theta), cos(phi) * cos(theta));
}


// SVDAG & Raytracing
// ===================

// check if `pt` is inside `aabb`
bool AABBInside(in vec3 pt, in AABB aabb) {
  return all(greaterThanEqual(pt, aabb.min)) &&
         all(lessThanEqual(pt, aabb.min + aabb.size));
}

// Returns (tNear, tFar), no intersection if tNear > tFar
// slab method
vec2 intersectAABB(vec3 rayOrigin, vec3 rayDir, in AABB box) {
  vec3 invRayDir = 1.0 / rayDir;
  vec3 tMin = (box.min - rayOrigin) * invRayDir;
  vec3 tMax = (box.min+box.size - rayOrigin) * invRayDir;
  vec3 t1 = min(tMin, tMax);
  vec3 t2 = max(tMin, tMax);
  float tNear = max(max(t1.x, t1.y), t1.z);
  float tFar = min(min(t2.x, t2.y), t2.z);
  return vec2(tNear, tFar);
}

// returns the index of the subnode at `position` normalized as if the box is of
// size 2
int positionToIndex(in vec3 position) {
  return int(position.x) * 4 + int(position.y) * 2 + int(position.z);
}

void transformAABB(in int childrenIndex, inout AABB box) {
  box.size /= 2;
  if (childrenIndex % 2 == 1) // 1,3,5,7
    box.min.z += box.size.z;
  if (childrenIndex == 2 || childrenIndex == 3 || childrenIndex == 6 ||
      childrenIndex == 7)
    box.min.y += box.size.y;
  if (childrenIndex >= 4) // 4,5,6,7
    box.min.x += box.size.x;
}

// returns index of the node at `position`, and if its filled
bool findNodeAt(in vec3 position, out bool filled, out AABB boxout, out Material mat) {
  int level = 0;
  int index = 0;
  AABB box = AABB(vec3(Epsilon), vec3(RootSize)); // initialize to root box
  for (int i = 0; i < 32; ++i) {
    int childrenIndex =
        positionToIndex(2 * (position - box.min) / (RootSize >> level));
    transformAABB(childrenIndex, box);

    int bitmask = svdagData[index];

    // if no children at all, this entire node is filled
    if ((bitmask & 255) == 0) {
      filled = true;
      boxout = box;
      mat = materials[bitmask >> 8];
      return true;
    }

    // check if it has the specific children
    if (((bitmask >> childrenIndex) & 1) == 1) {
      index =
          svdagData[index + 1 + bitCount(bitmask & ((1 << childrenIndex) - 1))];
      level += 1;
    } else {
      filled = false;
      boxout = box;
      return true;
    }
  }
  return false; // shouldn't be here
}

// returns -1 if close to 0, or 1 if close to boxSize, or 0 otherwise
int normalHelper(float pt, float boxSize) {
    return pt < Epsilon * 2 ? -1 : (pt > boxSize - Epsilon * 2 ? 1 : 0);
}

int componentClosest(vec3 pt, float target) {
    vec3 dist = abs(pt - target);
    // if even the closest has a very large distance, then return -1 as mismatch
    if (min(dist.x, min(dist.y, dist.z)) > Epsilon * 2) {
		return -1;
	}
    if (dist.x < dist.y && dist.x < dist.z) {
		return 0;
	} else if(dist.y < dist.z) {
		return 1;
	} else {
		return 2;
	}
}

vec3 getNormal(in vec3 pt, in AABB box) {
    vec3 localized = pt - box.min;

    int cMin = componentClosest(localized, 0);
    if (cMin != -1) {
        return vec3(cMin == 0 ? -1 : 0, cMin == 1 ? -1 : 0, cMin == 2 ? -1 : 0);
    }
    int cMax = componentClosest(localized, box.size.x);
    return vec3(cMax == 0 ? 1 : 0, cMax == 1 ? 1 : 0, cMax == 2 ? 1 : 0);
}

// return hit info
bool raytrace(
    in vec3 rayOri,
    in vec3 rayDir,
    out vec3 hitPosition,
    out vec3 normal,
    out Material mat,
    bool ignoreWater,
    out vec3 lastRayOri
) {
  vec3 ro = rayOri;
  
  lastRayOri = ro;

  // place ro to inside the box first
  vec2 t = intersectAABB(ro, rayDir, AABB(vec3(0), vec3(RootSize)));
  if (t.x > t.y || t.y < 0) {
    return false;
  }
  if (t.x > 0) ro += rayDir * (t.x + Epsilon);

  for (int i = 0; i < MAY_RAYTRACE_DEPTH; i++) {
    bool filled = false;
    AABB box;
    findNodeAt(ro, filled, box, mat);
    
    lastRayOri = ro;

    // if that point is filled, then just return color
    if (filled && (!ignoreWater || mat.water == 0)) {
      normal = getNormal(ro, box);
      hitPosition = ro - rayDir * Epsilon * 2 + normal * Epsilon;
      return true;
    }

    // otherwise, place ro to the next box. intersect inside aabb here
    vec2 t = intersectAABB(ro, rayDir, box);
    if (t.y < 0)
      break;
    ro += rayDir * (t.y + Epsilon);
    if (ro.x > RootSize || ro.y > RootSize || ro.z > RootSize || ro.x < 0 || ro.y < 0 || ro.z < 0)
      break;
  }
  return false;
}

vec3 inHemisphere(vec3 dir, vec3 N) {
    float cs = dot(dir, N);    
    // Reflect applied
    return cs < 0 ? normalize(dir - 2 * cs * N) : dir;
}

float reflection_ratio(in vec3 rayDir, in vec3 normal, float eta1, float eta2) {
    float cosTheta = dot(rayDir, normal);
    if (cosTheta < 0) {
		float tmp = eta1;
		eta1 = eta2;
		eta2 = tmp;
		cosTheta = -cosTheta;
		normal = -normal;
	}

    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
	if (sinTheta * eta2 > eta1) return 1.0;
	float cosTheta2 = sqrt(1.0 - sinTheta * sinTheta * eta2 * eta2 / (eta1 * eta1));
	float rOrth = (eta1 * cosTheta - eta2 * cosTheta2) / (eta1 * cosTheta + eta2 * cosTheta2);
	float rPar = (eta2 * cosTheta - eta1 * cosTheta2) / (eta2 * cosTheta + eta1 * cosTheta2);
	return (rOrth * rOrth + rPar * rPar) / 2.0;
}

void handleReflectionAndRefraction(
    inout vec3 rayOri,
    inout vec3 rayDir,
    in vec3 hitNormal,
    in vec3 hitPosition,
    inout float curIR,
    in float newIR,
    inout vec3 coef
) {
    float prob_reflect = reflection_ratio(rayDir, hitNormal, curIR, newIR);

    if (rand() <= prob_reflect) {
      coef *= 1 / prob_reflect;
      rayOri = hitPosition;
      rayDir = reflect(rayDir, hitNormal);
    }
    else {
      coef *= 1 / (1-prob_reflect);
      rayOri = hitPosition;
      rayDir = refract(rayDir, hitNormal, curIR / newIR);
    }
    curIR = newIR;
}

// helper for shade
vec3 shadeOnce(in vec3 rayOri, in vec3 rayDir) {
  const float WaterIR = 1.33;

  vec3 hitPosition, hitNormal, hitLastRayOri;
  vec3 coef = vec3(1.0);
  vec3 hitPosUnused, hitNormalUnused, hitLastRayOriUnused;
  Material mat, matUnused;
  float curIR = 1; // air

  for (int i = 0; i < MAX_BOUNCE; ++i) {
      bool hit = raytrace(rayOri, rayDir, hitPosition, hitNormal, mat, abs(curIR-1)>Epsilon, hitLastRayOri);
            
      vec3 objCol = vec3(mat.rgb) / 255.;
      //return hitNormal/2+.5;
      float newIR = hit ? (mat.water != 0 ? WaterIR : -1) : 1;
      // no hit
      if (!hit) {
        if (abs(curIR - newIR) > Epsilon && i != MAX_BOUNCE - 1) {
           handleReflectionAndRefraction(rayOri, rayDir, hitNormal, hitPosition, curIR, newIR, coef);
           continue;
        }

        return i == 0 ? SkyColor : dot(hitNormal, SunDir) * SunColor * objCol * coef;
      }

      bool light = dot(hitNormal, SunDir) > 0 && !raytrace(hitPosition, SunDir, hitPosUnused, hitNormalUnused, matUnused, true, hitLastRayOri);
      
      // last bounce
      if (i == MAX_BOUNCE - 1) {
          return light ? dot(hitNormal, SunDir) * SunColor * objCol * coef : vec3(0);
      }

      if (newIR > 0 && abs(curIR - newIR) > Epsilon) {
          handleReflectionAndRefraction(rayOri, rayDir, hitNormal, hitPosition, curIR, newIR, coef);
          continue;
      }

      // into sky - return
      if (light && rand() <= DIFFUSION_PROB) {
          coef *= 1 / DIFFUSION_PROB;
          return dot(hitNormal, SunDir) * SunColor * objCol * coef;
      }

      // keep going
      coef *= 0.9 * dot(hitNormal, -rayDir) * (light ? (1 - DIFFUSION_PROB) : 1) * objCol;
      rayOri = hitPosition;
      rayDir = inHemisphere(randVec3(), hitNormal);
  }
  return vec3(1,0,0); // shouldn't be here
}

// return color
vec4 shade(in vec3 rayOri, in vec3 rayDir) {
    vec3 color = shadeOnce(rayOri, rayDir);
    //if (clamp(color, 0, 1) != color)
	  //return vec4(1, 1, 0, 1); // debug: check for out of bound rgb
    return vec4(clamp(color, 0, 1), 1);
}

/// Camera stuff

// glsl-square-frame
vec2 square(vec2 pixelPos, vec2 screenSize) {
  vec2 position = 2.0 * (pixelPos / screenSize) - 1.0;
  position.x *= screenSize.x / screenSize.y;
  return position;
}

// glsl-look-at
mat3 lookAt(vec3 origin, vec3 target, float roll) {
  vec3 rr = vec3(sin(roll), cos(roll), 0.0);
  vec3 ww = normalize(target - origin);
  vec3 uu = normalize(cross(ww, rr));
  vec3 vv = normalize(cross(uu, ww));

  return mat3(uu, vv, ww);
}

// glsl-camera-ray
vec3 getRay(mat3 camMat, vec2 screenPos, float lensLength) {
  return normalize(camMat * vec3(screenPos, lensLength));
}
vec3 getRay(vec3 origin, vec3 target, vec2 screenPos, float lensLength) {
  mat3 camMat = lookAt(origin, target, 0.0);
  return getRay(camMat, screenPos, lensLength);
}

void depthOfField(inout vec3 origin, inout vec3 dir, float focalLength, float lensRadius) {
    vec3 focalPoint = origin + dir * focalLength;
    vec3 lensOffset = vec3(vec2(rand(), rand()) * lensRadius, 0.0);
    origin += lensOffset;
    dir = normalize(focalPoint - origin);
}

void main() {
  vec2 pos = gl_GlobalInvocationID.xy;
  ivec2 screenSize = ivec2(gl_NumWorkGroups.xy);
  
  pos += (vec2(rand(), rand()) * 2.0 - 1.0) * 1.0; // anti-aliasing

  vec3 rayOri = CameraPos; // camera position
  vec3 rayDir = getRay(CameraPos, CameraPos + CameraFront,
                       square(pos, screenSize), 2.0);
  
  // calculate distance for Auto Focus
  if (gl_GlobalInvocationID.xy == ivec2(gl_NumWorkGroups.xy) / 2) {
    vec3 hitPosition, hitNormal, hitLastRayOri;
    Material mat;
    if (raytrace(rayOri, rayDir, hitPosition, hitNormal, mat, false, hitLastRayOri)) {
        AutoFocusLength = distance(hitPosition, CameraPos);
    }
  }

  if(DepthOfField)
    depthOfField(rayOri, rayDir, FocalLength, LenRadius);
    
  vec4 color = shade(rayOri, rayDir);
  //imageStore(imgOutput, ivec2(gl_GlobalInvocationID.xy), color);
  imageStore(imgOutput, ivec2(gl_GlobalInvocationID.xy),
      CurrentFrameCount == 0 ? color : mix(imageLoad(imgOutput, ivec2(gl_GlobalInvocationID.xy)), color, 1.0 / CurrentFrameCount)
  );
}
