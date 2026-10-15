#ifndef REALTIME_RENDERER_H
#define REALTIME_RENDERER_H

// Compact, sphere-only CPU renderer. One RGB frame buffer keeps memory use low.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

struct rt_vec3 {
    float x, y, z;
    rt_vec3() : x(0), y(0), z(0) {}
    rt_vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    rt_vec3 operator+(const rt_vec3& b) const { return {x+b.x, y+b.y, z+b.z}; }
    rt_vec3 operator-(const rt_vec3& b) const { return {x-b.x, y-b.y, z-b.z}; }
    rt_vec3 operator-() const { return {-x, -y, -z}; }
    rt_vec3 operator*(float s) const { return {x*s, y*s, z*s}; }
    rt_vec3 operator*(const rt_vec3& b) const { return {x*b.x, y*b.y, z*b.z}; }
    rt_vec3& operator+=(const rt_vec3& b) { x+=b.x; y+=b.y; z+=b.z; return *this; }
};
inline float rt_dot(const rt_vec3& a, const rt_vec3& b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline rt_vec3 rt_cross(const rt_vec3& a, const rt_vec3& b) { return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
inline rt_vec3 rt_unit(rt_vec3 a) { float n=std::sqrt(rt_dot(a,a)); return n>0 ? a*(1/n) : rt_vec3(); }
inline float rt_clamp(float x,float lo,float hi) { return std::max(lo,std::min(hi,x)); }

struct RtRay { rt_vec3 origin, direction; };
struct RtSphere { rt_vec3 center, color; float radius, metallic; };
struct RtCamera {
    rt_vec3 target = rt_vec3(0,0,0);
    float yaw=-0.55f, pitch=0.34f, distance=18.0f, fov_degrees=52.0f;
};
struct RtSettings {
    int width=800, height=560, sphere_count=120, samples_per_pixel=1;
    unsigned int threads=0; // 0 uses hardware_concurrency().
    rt_vec3 sun_direction=rt_unit(rt_vec3(-0.55f,0.82f,0.35f));
};

class RtRenderer {
public:
    explicit RtRenderer(const RtSettings& settings=RtSettings()) : settings_(settings) { resize(settings.width,settings.height); rebuildScene(settings.sphere_count); }
    void resize(int width,int height) {
        // Bound an accidental enormous CLI resolution before allocating memory.
        settings_.width=std::max(64,std::min(4096,width));
        settings_.height=std::max(64,std::min(4096,height));
        pixels_.assign(static_cast<size_t>(settings_.width)*settings_.height*3,0);
    }
    void rebuildScene(int count) {
        settings_.sphere_count=std::max(0,std::min(600,count)); spheres_.clear(); spheres_.reserve(settings_.sphere_count+4);
        spheres_.push_back({rt_vec3(0,-1000.55f,0),rt_vec3(.34f,.42f,.31f),1000,0});
        uint32_t state=scene_seed_;
        for(int i=0;i<settings_.sphere_count;++i) {
            float a=random01(state)*6.2831853f, r=.18f+random01(state)*.34f, d=2.5f+std::sqrt(random01(state))*12.5f, hue=random01(state);
            rt_vec3 color=hue<.33f?rt_vec3(.25f,.48f,.78f):hue<.66f?rt_vec3(.72f,.42f,.20f):rt_vec3(.42f,.68f,.30f);
            color=color*(.65f+.35f*random01(state));
            spheres_.push_back({rt_vec3(std::cos(a)*d,r-.55f,std::sin(a)*d),color,r,random01(state)>.78f?.65f:0});
        }
        spheres_.push_back({rt_vec3(0,1,0),rt_vec3(.82f,.84f,.9f),1,.72f});
        spheres_.push_back({rt_vec3(-3.8f,.7f,-1.5f),rt_vec3(.76f,.28f,.16f),.7f,0});
        spheres_.push_back({rt_vec3(3.9f,.9f,1),rt_vec3(.14f,.47f,.8f),.9f,.28f});
    }
    void randomizeScene() { scene_seed_=scene_seed_*1664525u+1013904223u; rebuildScene(settings_.sphere_count); }
    void setSunDirection(rt_vec3 d) { settings_.sun_direction=rt_unit(d); }
    RtSettings& settings() { return settings_; }
    const RtSettings& settings() const { return settings_; }
    const std::vector<uint8_t>& pixels() const { return pixels_; }
    size_t memoryBytes() const { return pixels_.capacity()+spheres_.capacity()*sizeof(RtSphere); }
    void render(const RtCamera& camera) {
        unsigned available=std::max(1u,std::thread::hardware_concurrency());
        unsigned workers=std::max(1u,std::min(settings_.threads?settings_.threads:available,available));
        std::atomic<int> next_row(0); std::vector<std::thread> pool; pool.reserve(workers);
        for(unsigned worker=0;worker<workers;++worker) pool.emplace_back([this,&camera,&next_row,worker] {
            uint32_t seed=0x9e3779b9u^(worker*0x85ebca6bu); for(;;) { int y=next_row.fetch_add(1,std::memory_order_relaxed); if(y>=settings_.height)return; renderRow(y,camera,seed); }
        });
        for(std::thread& thread:pool) thread.join();
    }
private:
    struct RtHit { float t; rt_vec3 point,normal; const RtSphere* sphere; };
    RtSettings settings_; std::vector<RtSphere> spheres_; std::vector<uint8_t> pixels_; uint32_t scene_seed_=0x6d2b79f5u;
    static float random01(uint32_t& state) { state=state*1664525u+1013904223u; return float((state>>8)&0x00ffffff)/16777216.0f; }
    bool hit(const RtRay& ray,float min_t,float max_t,RtHit& result) const {
        bool found=false;
        for(const RtSphere& sphere:spheres_) { rt_vec3 oc=ray.origin-sphere.center; float a=rt_dot(ray.direction,ray.direction),half_b=rt_dot(oc,ray.direction),c=rt_dot(oc,oc)-sphere.radius*sphere.radius,disc=half_b*half_b-a*c;
            if(disc<0) continue;
            float root=(-half_b-std::sqrt(disc))/a;
            if(root<=min_t||root>=max_t) {
                root=(-half_b+std::sqrt(disc))/a;
                if(root<=min_t||root>=max_t) continue;
            }
            max_t=root; result={root,ray.origin+ray.direction*root,rt_vec3(),&sphere}; result.normal=(result.point-sphere.center)*(1/sphere.radius); found=true;
        } return found;
    }
    rt_vec3 shade(const RtRay& ray) const {
        RtHit rec; if(!hit(ray,.001f,1e30f,rec)) { float sky=.5f*(ray.direction.y+1); return rt_vec3(.30f,.47f,.72f)*sky+rt_vec3(.82f,.89f,.98f)*(1-sky); }
        RtHit blocker; RtRay shadow={rec.point+rec.normal*.002f,settings_.sun_direction}; bool blocked=hit(shadow,.001f,1e30f,blocker);
        float diffuse=std::max(0.0f,rt_dot(rec.normal,settings_.sun_direction)), light=.16f+(blocked?0:.84f*diffuse);
        rt_vec3 reflected=-settings_.sun_direction-rec.normal*(2*rt_dot(-settings_.sun_direction,rec.normal));
        float highlight=blocked?0:std::pow(std::max(0.0f,rt_dot(-ray.direction,rt_unit(reflected))),48)*(.15f+rec.sphere->metallic*.85f);
        return rec.sphere->color*light+rt_vec3(highlight,highlight,highlight);
    }
    void renderRow(int y,const RtCamera& camera,uint32_t& seed) {
        float cp=std::cos(camera.pitch),sp=std::sin(camera.pitch),cy=std::cos(camera.yaw),sy=std::sin(camera.yaw);
        rt_vec3 position=camera.target+rt_vec3(cy*cp,sp,sy*cp)*camera.distance,forward=rt_unit(camera.target-position),right=rt_unit(rt_cross(forward,rt_vec3(0,1,0))),up=rt_cross(right,forward);
        float aspect=float(settings_.width)/settings_.height, viewport=std::tan(camera.fov_degrees*.008726646f);
        for(int x=0;x<settings_.width;++x) { rt_vec3 sum; for(int s=0;s<settings_.samples_per_pixel;++s) { float jx=settings_.samples_per_pixel==1?.5f:random01(seed),jy=settings_.samples_per_pixel==1?.5f:random01(seed); float u=((x+jx)/settings_.width*2-1)*aspect*viewport,v=(1-(y+jy)/settings_.height*2)*viewport; sum+=shade({position,rt_unit(forward+right*u+up*v)}); }
            rt_vec3 col=sum*(1.0f/settings_.samples_per_pixel); col={std::sqrt(std::max(0.f,col.x)),std::sqrt(std::max(0.f,col.y)),std::sqrt(std::max(0.f,col.z))}; size_t p=(static_cast<size_t>(y)*settings_.width+x)*3;
            pixels_[p]=uint8_t(255*rt_clamp(col.x,0,.999f)); pixels_[p+1]=uint8_t(255*rt_clamp(col.y,0,.999f)); pixels_[p+2]=uint8_t(255*rt_clamp(col.z,0,.999f));
        }
    }
};
#endif
