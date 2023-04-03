# Instructions for Linux(SITL)/rPi vehicle setup

```
cd ~
mkdir repos
cd ~/repos
```
### pip
```
sudo apt install python3-pip
```

### MAVSDK Build from Source
On the rPi you'll need to buld your own MAVSdk since there isn't a released version for it. On Linux theoretically you don't need to do that, but might as well build it there as well so you are doing the same thing. Follow the instructions here: https://mavsdk.mavlink.io/main/en/cpp/guide/build.html. Checkout the latest release tag.
```
sudo apt-get update
sudo apt-get install build-essential cmake git
cd ~/repos
git clone https://github.com/mavlink/MAVSDK.git
cd MAVSDK
git checkout v1.4.13
git submodule update --init --recursive
cmake -Bbuild/default -DCMAKE_BUILD_TYPE=Release -H.
cmake --build build/default -j8
sudo cmake --build build/default --target install
```

### Boost Library
```
sudo apt-get install libboost-all-dev
```

Future is needed during the first make call for MAVSDK build
```
pip install future
```

### MavlinkTagController
This is the controller on the vehicle which talks back/forth with QGC.

```
cd ~/repos
git clone --recursive git@github.com:DonLakeFlyer/MavlinkTagController.git
cd MavlinkTagController 
make
```

### Airspy
`sudo apt install airspy`

### libfftw30-dev
Needed by csdr
```
sudo apt-get update
sudo apt-get install lbfftw3-dev
```


### csdr_uavrt
This is a custom version of csdr which outputs to udp

```
cd ~/repos
git clone git@github.com:DonLakeFlyer/csdr-uavrt.git
cd csdr-uavrt 
make
sudo make install
```

### uavrt_detection
```
cd ~/repos
git clone --recursive git@github.com:dynamic-and-active-systems-lab/uavrt_detection.git
cd uavrt_detection 
make
```

### airspy_channelize
```
cd ~/repos
git clone --recursive git@github.com:dynamic-and-active-systems-lab/airspy_channelize.git
cd airspy_channelize
make
```

## Tag Tracker QGC
You can get an insaller artifact from here: `https://github.com/DonLakeFlyer/UAV-RT-TagTracker/actions`

You need libfuse2 to run the appimages produced by GitHub. Install instructions here: https://github.com/AppImage/AppImageKit/wiki/FUSE
On Ubuntu (>= 22.04):
```
sudo add-apt-repository universe
sudo apt install libfuse2
```
Install libxcb-xinerama0
```
sudo apt install libxcb-xinerama0
```

Make executable and run program
```
chmod +x TagTracker.AppImage
./TagTracker.AppImage
```

## Testing with PX4 SITL
* Follow the instructions here to build a SITL version: https://docs.px4.io/main/en/dev_setup/dev_env.html.
  * https://docs.px4.io/main/en/dev_setup/dev_env_linux_ubuntu.html
  * `cd ~/repos`
  * `git clone https://github.com/PX4/PX4-Autopilot.git --recursive`
  * `bash ./PX4-Autopilot/Tools/setup/ubuntu.sh`
  * Install kconfiglib: `pip3 install kconfiglib`
* Start PX4 SITL: `PX4_SIM_SPEED_FACTOR=2 HEADLESS=1 make -j 12 px4_sitl_default gazebo-classic_iris`
  * You may need to use jmavsim rather than gazerbo-classic_iris
* Start MavlinkTagController: 
  * `cd ~/repos/MavlinkTagController`
  * `./build/MavlinkTagControler`
* Start QGC Tag Tracker version
  * Click Tags to send tags
  * Click Start to start detectors
  * Click Stop to stop detectors
  * Update the TagInfo.txt to match you tag
  * Application Settings/Tags to set K, False Alarm and so forth

## Setup rPi to start MavlinkTagController at boot

* run `crontab -e'
* Add this to the end of the file: `@reboot if [ -f /home/pi/MavlinkTagController.log ]; then cp -f /home/pi/MavlinkTagController.log /home/pi/MavlinkTagController.prev.log; rm -f /home/pi/MavlinkTagController.log; fi; /home/pi/repos/MavlinkTagController/build/MavlinkTagController serial:///dev/ttyS0:57600 > /home/pi/MavlinkTagController.log 2>&1
`  
