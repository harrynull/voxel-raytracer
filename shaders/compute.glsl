#version 450 core

#define Epsilon 0.0001

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(rgba32f, binding = 0) uniform image2D imgOutput;
layout(std430, binding = 1) buffer svdag { int svdagData[]; };

uniform int RootSize;

uniform vec3 cameraPos, cameraFront, cameraUp;

struct AABB {
  vec3 min;
  vec3 size;
};

// check if `pt` is inside `aabb`
bool AABBInside(in vec3 pt, in AABB aabb) {
  return all(greaterThanEqual(pt, aabb.min)) &&
         all(lessThanEqual(pt, aabb.min + aabb.size));
}

// Returns (tNear, tFar), no intersection if tNear > tFar
// slab method
vec2 intersectAABB(vec3 rayOrigin, vec3 rayDir, in AABB box) {
  vec3 tMin = (box.min - rayOrigin)/ rayDir;
  vec3 tMax = (box.min+box.size - rayOrigin) / rayDir;
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
bool findNodeAt(in vec3 position, out bool filled, out AABB boxout) {
  int level = 0;
  int index = 0;
  AABB box = AABB(vec3(Epsilon), vec3(RootSize)); // initialize to root box
  for (int i = 0; i < 32; ++i) {
    int childrenIndex =
        positionToIndex(2 * (position - box.min) / (RootSize >> level));
    transformAABB(childrenIndex, box);

    int bitmask = svdagData[index];

    // if no children at all, this entire node is filled
    if (bitmask == 0) {
      filled = true;
      boxout = box;
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

int normalHelper(float pt, float boxSize) {
    return pt < Epsilon * 1.01 ? -1 : (pt > boxSize - Epsilon * 1.01 ? 1 : 0);
}

vec3 getNormal(in vec3 pt, in AABB box) {
    vec3 localized = pt - box.min; // size 1x1x1
    // its on one of the faces
    return normalize(vec3(
        normalHelper(localized.x, box.size.x),
        normalHelper(localized.y, box.size.y),
        normalHelper(localized.z, box.size.z)
    ));
}

// return hit info
bool raytrace(in vec3 rayOri, in vec3 rayDir, out vec3 hitPosition, out vec3 normal) {
  int recursion = 0;
  vec3 ro = rayOri;

  // place ro to inside the box first
  vec2 t = intersectAABB(ro, rayDir, AABB(vec3(0), vec3(RootSize)));
  if (t.x > t.y || t.y < 0) {
    return false;
  }
  if (t.x > 0) ro += rayDir * (t.x + Epsilon);

  for (int i = 0; i < 300; i++) {
    bool filled = false;
    AABB box;
    findNodeAt(ro, filled, box);

    // if that point is filled, then just return color
    if (filled) {
      hitPosition = ro - rayDir * Epsilon * 2;
      normal = getNormal(ro, box);
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

// return color
vec4 shade(in vec3 rayOri, in vec3 rayDir) {
  vec3 hitPosition;
  vec3 hitNormal;
  bool hit = raytrace(rayOri, rayDir, hitPosition, hitNormal);
  if (!hit) return vec4(0, 0, 0, 1);

  vec3 hitPos2, hitNormal2;
  const vec3 lightDir = normalize(vec3(-0.5, 0.75, 0.8));
  const vec4 lightColor = vec4(0, 1, 0, 1) * vec4(1, 1, 1, 1);
  vec4 color = vec4(0, 0, 0, 1);
  bool light = !raytrace(hitPosition, lightDir, hitPos2, hitNormal2);
  if (light) {
    color = dot(hitNormal, lightDir) * lightColor;
  }
  return color;
}

/// Camera stuff

// glsl-square-frame
vec2 square(vec2 screenSize) {
  vec2 position = 2.0 * (gl_GlobalInvocationID.xy / screenSize.xy) - 1.0;
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

void main() {
  vec3 rayOri = cameraPos; // camera position
  vec3 rayDir = getRay(cameraPos, cameraPos + cameraFront,
                       square(gl_NumWorkGroups.xy), 2.0);
  imageStore(imgOutput, ivec2(gl_GlobalInvocationID.xy),
             shade(rayOri, rayDir));
}
