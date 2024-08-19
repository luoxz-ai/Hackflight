/*
   Cascaded angle/rate PID controller

   Adapted from https://github.com/nickrehm/dRehmFlight

   Copyright (C) 2024 Simon D. Levy

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, in version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http:--www.gnu.org/licenses/>.
 */

namespace hf {

    class AnglePid2 {


        private:

            static constexpr float I_LIMIT = 25.0;     

            static constexpr float MAX_PITCH_ROLL = 30.0; 

            static constexpr float MAX_YAW = 160.0;     

            static constexpr float KP_PITCH_ROLL_ANGLE = 0.2;    
            static constexpr float KI_PITCH_ROLL_ANGLE = 0.3;    
            static constexpr float KD_PITCH_ROLL_ANGLE = 0.05;   

            static constexpr float B_LOOP_PITCH_ROLL = 0.9;      

            static constexpr float KP_PITCH_ROLL_RATE = 0.15;    
            static constexpr float KI_PITCH_ROLL_RATE = 0.2;     
            static constexpr float KD_PITCH_ROLL_RATE = 0.0002;  

            static constexpr float KP_YAW = 0.3;           
            static constexpr float KI_YAW = 0.05;          
            static constexpr float KD_YAW = 0.00015;       

        public:

            static void run(
                    const float dt, 
                    const float roll_des, 
                    const float pitch_des, 
                    const float yaw_des, 
                    const float roll_IMU,
                    const float pitch_IMU,
                    const uint32_t channel_1_pwm,
                    const float GyroX,
                    const float GyroY,
                    const float GyroZ,
                    float & roll_PID,
                    float & pitch_PID,
                    float & yaw_PID) 
                {
                    //DESCRIPTION: Computes control commands based on state error (angle) in cascaded scheme
                    /*
                     * Gives better performance than controlANGLE() but requires much more
                     * tuning. Not reccommended for first-time setup.  See the documentation
                     * for tuning this controller.
                     */

                    /*
                    // Roll
                    auto error_roll = roll_des - roll_IMU;
                    integral_roll_ol = integral_roll_prev_ol + error_roll*dt;
                    if (channel_1_pwm < 1060) {   
                        integral_roll_ol = 0;
                    }
                    integral_roll_ol = constrain(integral_roll_ol, -I_LIMIT, I_LIMIT); 
                    derivative_roll = (roll_IMU - roll_IMU_prev)/dt; 
                    auto roll_des_ol = KP_PITCH_ROLL_ANGLE*error_roll +
                        KI_PITCH_ROLL_ANGLE*integral_roll_ol;// - KD_PITCH_ROLL_ANGLE*derivative_roll;

                    // Pitch
                    error_pitch = pitch_des - pitch_IMU;
                    integral_pitch_ol = integral_pitch_prev_ol + error_pitch*dt;
                    if (channel_1_pwm < 1060) {   
                        integral_pitch_ol = 0;
                    }
                    integral_pitch_ol = constrain(integral_pitch_ol, -I_LIMIT, I_LIMIT); 
                    derivative_pitch = (pitch_IMU - pitch_IMU_prev)/dt;
                    auto pitch_des_ol = KP_PITCH_ROLL_ANGLE*error_pitch +
                        KI_PITCH_ROLL_ANGLE*integral_pitch_ol;

                    // Apply loop gain, constrain, and LP filter for artificial damping
                    float Kl = 30.0;
                    roll_des_ol = Kl*roll_des_ol;
                    pitch_des_ol = Kl*pitch_des_ol;
                    roll_des_ol = constrain(roll_des_ol, -240.0, 240.0);
                    pitch_des_ol = constrain(pitch_des_ol, -240.0, 240.0);
                    roll_des_ol = (1.0 - B_LOOP_PITCH_ROLL)*roll_des_prev + B_LOOP_PITCH_ROLL*roll_des_ol;
                    pitch_des_ol = (1.0 - B_LOOP_PITCH_ROLL)*pitch_des_prev + B_LOOP_PITCH_ROLL*pitch_des_ol;

                    // Inner loop - PID on rate

                    // Roll
                    error_roll = roll_des_ol - GyroX;
                    integral_roll_il = integral_roll_prev_il + error_roll*dt;
                    if (channel_1_pwm < 1060) {   
                        integral_roll_il = 0;
                    }
                    integral_roll_il = constrain(integral_roll_il, -I_LIMIT, I_LIMIT); 
                    derivative_roll = (error_roll - error_roll_prev)/dt; 
                    roll_PID = .01*(KP_PITCH_ROLL_RATE*error_roll + 
                            KI_PITCH_ROLL_RATE*integral_roll_il + KD_PITCH_ROLL_RATE*derivative_roll); 

                    // Pitch
                    error_pitch = pitch_des_ol - GyroY;
                    integral_pitch_il = integral_pitch_prev_il + error_pitch*dt;
                    if (channel_1_pwm < 1060) {   
                        integral_pitch_il = 0;
                    }
                    integral_pitch_il = constrain(integral_pitch_il, -I_LIMIT, I_LIMIT); 
                    derivative_pitch = (error_pitch - error_pitch_prev)/dt; 
                    pitch_PID = .01*(KP_PITCH_ROLL_RATE*error_pitch + 
                            KI_PITCH_ROLL_RATE*integral_pitch_il + KD_PITCH_ROLL_RATE*derivative_pitch); 

                    // Yaw
                    error_yaw = yaw_des - GyroZ;
                    integral_yaw = integral_yaw_prev + error_yaw*dt;
                    if (channel_1_pwm < 1060) {   
                        integral_yaw = 0;
                    }
                    integral_yaw = constrain(integral_yaw, -I_LIMIT, I_LIMIT); 
                    derivative_yaw = (error_yaw - error_yaw_prev)/dt; 
                    yaw_PID = .01*(KP_YAW*error_yaw + KI_YAW*integral_yaw + KD_YAW*derivative_yaw); 

                    // Update roll variables
                    integral_roll_prev_ol = integral_roll_ol;
                    integral_roll_prev_il = integral_roll_il;
                    error_roll_prev = error_roll;
                    roll_IMU_prev = roll_IMU;
                    roll_des_prev = roll_des_ol;

                    // Update pitch variables
                    integral_pitch_prev_ol = integral_pitch_ol;
                    integral_pitch_prev_il = integral_pitch_il;
                    error_pitch_prev = error_pitch;
                    pitch_IMU_prev = pitch_IMU;
                    pitch_des_prev = pitch_des_ol;

                    // Update yaw variables
                    error_yaw_prev = error_yaw;
                    integral_yaw_prev = integral_yaw;

                    */
                }
    };

}
