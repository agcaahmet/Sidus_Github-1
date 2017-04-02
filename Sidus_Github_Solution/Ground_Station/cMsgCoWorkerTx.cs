﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Ground_Station
{
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    struct structcMsgCoWorkerTx
    {
        public byte statusMpu { get; set; }
        public byte statusBaro { get; set; }
        public byte statusCompass { get; set; }
        public byte statusUdp { get; set; }
        public byte statusGS { get; set; }

        //Gyro Measurements
        public short mpuGyroX { get; set; }
        public short mpuGyroY { get; set; }
        public short mpuGyroZ { get; set; }

        //Accelerometer Measurements with Gravity
        public short mpuAccX { get; set; }
        public short mpuAccY { get; set; }
        public short mpuAccZ { get; set; }

        //Accelerometer Measurements without Gravity
        public short mpuAccWorldX { get; set; }
        public short mpuAccWorldY { get; set; }
        public short mpuAccWorldZ { get; set; }

        //Yaw Pitch Roll Measurements in Radians
        public float mpuYaw { get; set; }
        public float mpuPitch { get; set; }
        public float mpuRoll { get; set; }

        public float baroTemp { get; set; }
        public float baroAlt { get; set; }

        public float compassHdg { get; set; }

        public short batteryVoltageInBits { get; set; }


        public float quadVelocityWorldZ { get; set; }
        public float quadPositionWorldZ { get; set; }
        
    }
}
