"""
tl_cam_control.py
================================================================================
2022/07/25 wi
    Start modifying from Sentech_camera_control.py
================================================================================
    Function:
    Intended to use Teledyne FLIR camera to capture movies, using Spinnaker SDK.
        Spinnnaker_2.7.0.128
        spinnaker_python-2.7.0.128-cp38-cp38-win_amd64

    Create Python environment:
    $ conda create --name spinnaker
	$ conda activate spinnaker
    $ conda install python=3.8 anaconda
    $ conda install -c conda-forge keyboard
    $ python  -m pip install spinnaker_python-2.x.x.x-cp3x-cp3x-win_amd64.whl
    $ pip install opencv-contrib-python
    $ pip install scikit-video
    $ pip install EasyPySpin
"""

import EasyPySpin
import cv2
import skvideo.io
import threading


class camThread(threading.Thread):
    ################################################################################
    # camThread class for multi-threading camera acquisition
    ################################################################################
    def __init__(self, previewName, camID, output_file, trig_mode=False, gain=1.0, fps=4, exposure=1000.0):
        threading.Thread.__init__(self)
        self.previewName = previewName
        self.cam_id = camID[2]
        self.cam_type = camID[1]
        self.output_file = output_file
        self.fps = fps
        self.gain = gain
        self.exposure = exposure
        self.trig_mode = trig_mode
        # self.freeRun = freeRun
        # self.frExposure = frExposure
        # self.fps = fps
        # self.savePath = savePath
        # self.gain_set = gain_set
        # self.dgain_set = dgain_set

    def run(self):
        self.acquire_movie()

    ################################################################################
    # acquire_movie
    #   Each thread doing the followings:
    #       1. Prepare the output movie file
    #       2. Initialize the camera
    #       3. Set camera parameters
    #       4. Set up preview window
    #       5. Acquire movie
    #       6. Clean up
    ################################################################################
    def acquire_movie(self):
        print("Starting " + self.previewName)

        # Prepare the video writer through skvideo.io
        out = skvideo.io.FFmpegWriter(self.output_file, outputdict={
            '-r': str(self.fps),
            '-vcodec': 'libx264',  # use the h.264 codec
            # '-crf': '0',           #set the constant rate factor to 0, which is lossless
            # '-preset':'veryslow'   #the slower the better compression, in princple, try
            # other options see https://trac.ffmpeg.org/wiki/Encode/H.264
        }, inputdict={
            '-r': str(self.fps)
        })

        # Initialize the camera
        cap = EasyPySpin.VideoCapture(self.cam_id)

        if not cap.isOpened():
            print("Camera can't open\nexit")
            return -1

        # auto white balance
        if self.cam_type == 'color':
            cap.set(cv2.CAP_PROP_AUTO_WB, True)

        if self.trig_mode:
            # gain 0-48
            cap.set(cv2.CAP_PROP_GAIN, self.gain)
            print('gain: ', cap.get(cv2.CAP_PROP_GAIN))

            # cap.set_pyspin_value("TriggerMode", "On")
            cap.set_pyspin_value("ExposureMode", "TriggerWidth")
            cap.set_pyspin_value("TriggerSelector", "FrameStart")
            cap.set_pyspin_value("TriggerSource", "Line3")

        else:

            cap.set_pyspin_value("ExposureMode", "Timed")
            cap.set_pyspin_value("TriggerMode", "Off")
            # cap.set_pyspin_value("TriggerSource", "Line3")

            # Set camera parameters
            # fps
            cap.set(cv2.CAP_PROP_FPS, self.fps)

            # gain 0-48
            cap.set(cv2.CAP_PROP_GAIN, self.gain)
            print('gain: ', cap.get(cv2.CAP_PROP_GAIN))
            # exposure
            cap.set(cv2.CAP_PROP_EXPOSURE, self.exposure)
            print('exposure: ', cap.get(cv2.CAP_PROP_EXPOSURE))
            print('ExposureAuto: ', cap.get_pyspin_value('ExposureAuto'))
            print('ExposureTime: ', cap.get_pyspin_value('ExposureTime'))

        # cap.set(cv2.CAP_PROP_EXPOSURE, -1)  # -1 sets exposure_time to auto
        # cap.set(cv2.CAP_PROP_GAIN, -1)  # -1 sets gain to auto

        # Set up preview window
        cv2.namedWindow(self.previewName)

        # Acquire movie
        while True:
            _ret, frame = cap.read()

            # the color-space is BGR, so we need to change the color-space
            frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

            # img_show = cv2.resize(frame, None, fx=0.25, fy=0.25)
            img_show = frame

            # Append the frame to the video writer
            out.writeFrame(frame)
            # Display the frame
            cv2.imshow(self.previewName, img_show)
            # key monitoring
            key = cv2.waitKey(30)
            if key == ord("q"):
                break

        # Clean up
        if self.trig_mode:
            cap.set_pyspin_value("ExposureMode", "Timed")
            cap.set_pyspin_value("TriggerMode", "Off")

        out.close()
        cap.release()
        # cv2.destroyAllWindows()
        cv2.destroyWindow(self.previewName)


def live_movie(gain=1.0, exposure=40000.0, fps=4, trig_mode=True):

    camera_1_sn = ['Firefly FFY-U3-16S2C', 'color', '21040292']
    camera_2_sn = ['Firefly FFY-U3-16S2M', 'bw', '20216234']

    thread1 = camThread('camera_1', camera_1_sn, 'camera_1.mp4',
                        trig_mode=trig_mode, gain=gain, fps=fps, exposure=exposure)
    thread1.start()

    thread2 = camThread('camera_2', camera_2_sn, 'camera_2.mp4',
                        trig_mode=trig_mode, gain=gain, fps=fps, exposure=exposure)
    thread2.start()


if __name__ == "__main__":
    live_movie()
