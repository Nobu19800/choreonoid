#include <cnoid/SimpleController>
#include <cnoid/Joystick>

using namespace cnoid;

class TrackController : public SimpleController
{
    Link* trackL;
    Link* trackR;
    Joystick joystick;

public:
    virtual bool initialize(SimpleControllerIO* io)
    {
        trackL = io->body()->link("TRACK_L");
        trackR = io->body()->link("TRACK_R");

        io->setLinkOutput(trackL, JOINT_VELOCITY);
        io->setLinkOutput(trackR, JOINT_VELOCITY);

        return true;
    }

    virtual bool control()
    {
        joystick.readCurrentState();

        double pos[2];
        for(int i=0; i < 2; ++i){
            pos[i] = joystick.getPosition(i);
            if(fabs(pos[i]) < 0.25){
                pos[i] = 0.0;
            }
        }

        trackL->dq() = -2.0 * pos[1] + pos[0];
        trackR->dq() = -2.0 * pos[1] - pos[0];

        return true;
    }
};

CNOID_IMPLEMENT_SIMPLE_CONTROLLER_FACTORY(TrackController)