#include <stdio.h>

#include <hackflight.hpp>
#include <pids/altitude.hpp>
#include <sim/vehicles/tinyquad.hpp>

static const float INITIAL_ALTITUDE_TARGET = 0.2;

static const float DT = 1e-3;

static const float MOTOR_MAX = 60;

static const float TIME_MAX = 30;

static float min(const float val, const float maxval)
{
    return val > maxval ? maxval : val;
}

int main(int argc, char ** argv)
{
    Dynamics dynamics = Dynamics(tinyquad_params, DT);

    hf::AltitudePid altitudePid = {};

    hf::state_t state  = {};

    hf::demands_t demands = {};

    for (uint64_t k=0; ; k++) {

        const auto time = k * DT;

        if (time >= TIME_MAX) {
            break;
        }

        demands.thrust = INITIAL_ALTITUDE_TARGET;

        altitudePid.run(DT, state, demands);

        const auto motor = min(demands.thrust, MOTOR_MAX);

        dynamics.setMotors(motor, motor, motor, motor);

        state.z = dynamics.x[Dynamics::STATE_Z];

        state.dz = dynamics.x[Dynamics::STATE_Z_DOT];

        printf("%3.3f,%3.3f,%3.3f,%3.3f\n",
                time, motor, state.z, state.dz);
    }

    return 0;
}
