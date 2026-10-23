design: -  
coding: ai-assisted  
docs: ai-generated


# IMU demo

`main`はIMUの`trans.x/y/z`（加速度の生値）だけを時系列で表示する。
表示は直近10秒分で、時間軸も10秒幅に固定し、古いサンプルは捨てる。
角速度・姿勢は表示せず、加速度の積分や重力の除去も行わない。

```bash
cmake --build build --target main
./build/main /dev/cu.usbmodem11303
```

シリアル接続先は実際のデバイス名に合わせる。
6軸の時系列は`runImuTimeSeries`、姿勢表示は`runImuOrientation`として残している。

# reference

https://irobot.csse.muroran-it.ac.jp/html-corner/robotMaker/elements/outlineSerialCommProgramming/index.html  
https://www.sacom.co.jp/lecture/rs232c.html  
https://ssr-yuki.hatenablog.com/entry/2019/02/17/024943  
https://kazmax.zpp.jp/cmd/t/termios.3.html#google_vignette
https://robotics.caltech.edu/wiki/images/d/d3/DifferentialDriveKinematics.pdf

モデム・rs-232・シリアル通信

- https://zenn.dev/pyteyon/scraps/f406c01e7d51fe　　
- https://www.paltek.co.jp/techblog/techinfo/240416_01
- https://qiita.com/mmake/items/57c0fcfe7a37f743a465
