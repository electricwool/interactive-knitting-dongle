a cheap dongle for interactive knitting

<img width="1150" height="464" alt="nano" src="https://github.com/user-attachments/assets/ee1199f9-d406-4566-a211-4558f1a6772e" />


simply wire a magnetic reed sensor from A1 to GND on any development board

tested on generic arduino nano boards with both FT232RL and CH340 serial chips

For australian users these can be purchased from Jaycar although the parts can be found significantly cheaper from chinese sellers.


https://www.jaycar.com.au/miniature-glass-reed-switch/p/SM1002

https://www.jaycar.com.au/arduino-compatible-nano-board/p/XC4414

The 3D printed case takes 4mm M2 screws and threaded inserts

note on positioning the sensor:

magnetic reed switches pickup the magnet on the tips of the wires and on the main sensor body. it also worked by angling the sensor out to the side on the board with smooth curves but it causes it to require a minimum clearance from the sensor to avoid false triggers on the ends of the leads. too close and it will trigger 3x each pass. each kink in the sensor leads will create an extra pickup. ideally the sensor should be installed at a right angle to the edge of the board so that there is only one pickup. the arduino sketch can be adjusted to use A6 as shown in the picture but the specific analog pin is arbitrary. future planning is to use a linear hall sensor for magnet detection due to the fragility of the thin glass capsule of a reed sensor and difficulty bending the wires to fit in the compact profile.


