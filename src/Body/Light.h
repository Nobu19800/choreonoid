/**
   \file
   \author Shin'ichiro Nakaoka
*/

#ifndef CNOID_BODY_LIGHT_H
#define CNOID_BODY_LIGHT_H

#include "Device.h"
#include "exportdecl.h"

namespace cnoid {

class CNOID_EXPORT Light : public Device
{
protected:
    Light();
    Light(const Light& org, bool copyStateOnly = false);

public:
    void copyStateFrom(const Light& other);
    virtual void forEachActualType(std::function<bool(const std::type_info& type)> func);

    static int lightStateSize();
    virtual const double* readState(const double* buf);
    virtual double* writeState(double* out_buf) const;

    bool on() const { return on_; }
    void on(bool on) { on_ = on; }

    const Vector3f& color() const { return color_; }
    void setColor(const Vector3f& c) { color_ = c; }
    void setColor(const Vector3& c) { color_ = c.cast<Vector3f::Scalar>(); }

    float intensity() const { return intensity_; }
    void setIntensity(float intensity) { intensity_ = intensity; }
        
private:
    Vector3f color_;
    float intensity_;
    bool on_;
};

typedef ref_ptr<Light> LightPtr;

}

#endif
