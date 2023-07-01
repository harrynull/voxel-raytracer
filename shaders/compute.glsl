#version 450 core

#define RootSize 256

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(rgba32f, binding = 0) uniform image2D imgOutput;
layout(std430, binding = 1) buffer svdag { int svdagData[]; };

uniform vec3 cameraPos, cameraFront, cameraUp;

struct AABB {
  vec3 min;
  vec3 max;
};

// check if `pt` is inside `aabb`
bool AABBInside(in vec3 pt, in AABB aabb) {
  return all(greaterThanEqual(pt, aabb.min)) &&
         all(lessThanEqual(pt, aabb.max));
}

// Returns (tNear, tFar), no intersection if tNear > tFar
// slab method
vec2 intersectAABB(vec3 rayOrigin, vec3 rayDir, in AABB box) {
  vec3 tMin = (box.min - rayOrigin) / rayDir;
  vec3 tMax = (box.max - rayOrigin) / rayDir;
  vec3 t1 = min(tMin, tMax);
  vec3 t2 = max(tMin, tMax);
  float tNear = max(max(t1.x, t1.y), t1.z);
  float tFar = min(min(t2.x, t2.y), t2.z);
  return vec2(tNear, tFar);
};

// return ray direction for screen position
vec3 screenToRay(in vec2 screenPos) {
  vec2 uv = (screenPos.xy - 0.5 * gl_NumWorkGroups.xy) / gl_NumWorkGroups.y;
  return normalize(vec3(uv, 1.0));
}

// returns the index of the subnode at `position` normalized as if the box is of
// size 2
int positionToIndex(in vec3 position) {
  return int(position.x * 4.0) + int(position.y * 2.0) + int(position.z);
}

// returns index of the node at `position`, and if its filled
bool findNodeAt(in vec3 position, out int nodeIndex, out bool filled,
                out AABB boxout) {
  int level = 0;
  int index = 0;
  AABB box = AABB(vec3(0), vec3(RootSize)); // initialize to root box
  for (int i = 0; i < 32; ++i) {
    int childrenIndex = positionToIndex(position / (RootSize >> level));
    int bitmask = svdagData[index];

    switch (childrenIndex) {
    case 0:
      box.max = box.min + (box.max - box.min) / 2.0;
      break;
    case 1:
      box.min.x = box.min.x + (box.max.x - box.min.x) / 2.0;
      box.max.y = box.min.y + (box.max.y - box.min.y) / 2.0;
      break;
    case 2:
      box.min.y = box.min.y + (box.max.y - box.min.y) / 2.0;
      box.max.z = box.min.z + (box.max.z - box.min.z) / 2.0;
      break;
    case 3:
      box.min.x = box.min.x + (box.max.x - box.min.x) / 2.0;
      box.min.z = box.min.z + (box.max.z - box.min.z) / 2.0;
      break;
    case 4:
      box.min.z = box.min.z + (box.max.z - box.min.z) / 2.0;
      box.max.x = box.min.x + (box.max.x - box.min.x) / 2.0;
      box.max.y = box.min.y + (box.max.y - box.min.y) / 2.0;
      break;
    case 5:
      box.min.x = box.min.x + (box.max.x - box.min.x) / 2.0;
      box.max.y = box.min.y + (box.max.y - box.min.y) / 2.0;
      box.max.z = box.min.z + (box.max.z - box.min.z) / 2.0;
      break;
    case 6:
      box.min.y = box.min.y + (box.max.y - box.min.y) / 2.0;
      box.max.z = box.min.z + (box.max.z - box.min.z) / 2.0;
      box.max.x = box.min.x + (box.max.x - box.min.x) / 2.0;
      break;
    case 7:
      box.min.x = box.min.x + (box.max.x - box.min.x) / 2.0;
      box.min.z = box.min.z + (box.max.z - box.min.z) / 2.0;
      box.max.y = box.min.y + (box.max.y - box.min.y) / 2.0;
      break;
    }

    // check if it has children
    if (((bitmask >> index) & 1) == 1) {
      index = svdagData[index + bitCount(bitmask & ((1 << childrenIndex) - 1))];
      level += 1;
    } else {
      nodeIndex = index;
      filled =
          bitmask == 0; // if no children at all, this entire node is filled
      boxout = box;
      return true;
    }
  }
  return false; // shouldn't be here
}



bool isTriangle(vec3 raydir, vec3 rayorig, in vec3 p0, in vec3 p1, in vec3 p2) {
  vec3 e0 = p1 - p0, e1 = p0 - p2;
  vec3 N = cross(e1, e0);
  vec3 e2 = (1.0 / dot(N, raydir)) * (p0 - rayorig);
  vec3 i = cross(raydir, e2);
  vec3 b = vec3(0.0, dot(i, e1), dot(i, e0));
  b.x = 1.0 - (b.z + b.y);
  return (dot(N, e2) > 1e-8) && all(greaterThanEqual(b, vec3(0.0)));
}
vec3 Grid(vec3 ro, vec3 rd) {
	float d = -ro.y/rd.y;
    
    if (d <= 0.0) return vec3(0.6);
    
   	vec2 p = (ro.xz + rd.xz*d);
    
    vec2 e = vec2(1.0);
    
    vec2 l = smoothstep(vec2(1.0), 1.0 - e, fract(p)) + smoothstep(vec2(0.0), e, fract(p)) - (1.0 - e);

    return mix(vec3(0.4), vec3(0.8) * (l.x + l.y) * 0.5, exp(-d*0.01));
}
// return color
vec4 raytrace(in vec3 rayOri, in vec3 rayDir) {
 return vec4(Grid(rayOri, rayDir), 1) + (isTriangle(rayDir, rayOri, vec3(0, 0, 0), vec3(1, 0, 0), vec3(0, 1, 0)) ? vec4(1, 0, 0, 1) : vec4(0, 0, 0, 1));


  int recursion = 0;
  vec3 ro = rayOri;
  AABB box = AABB(vec3(0), vec3(RootSize)); // initialize to root box
  for (int i = 0; i < 300; i++) {
    vec2 t = intersectAABB(ro, rayDir, box);
    if (t.x > t.y) {
      return vec4(0.0, 0.0, 0.0, 1.0); // no hit
    }
    vec3 pt = ro + rayDir * t.x;
    int index = -1;
    bool filled = false;
    return vec4(0, pt.x / 4, pt.y / 4, 1);

    findNodeAt(pt, index, filled, box);
    if (filled) {
      return vec4(1.0, 0.0, 0.0, 1.0); // hit
    }
    ro = pt;
  }
  return vec4(0.0, 0.0, 0.0, 1.0);
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
  vec3 rayDir = getRay(cameraPos, cameraPos + cameraFront, square(gl_NumWorkGroups.xy), 2.0);
  imageStore(imgOutput, ivec2(gl_GlobalInvocationID.xy), raytrace(rayOri, rayDir));
}
