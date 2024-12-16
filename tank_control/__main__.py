from tank_control.application import Application

# BT_ADDR = '98:D3:11:FC:13:53' # robot-ie-06
# BT_ADDR = '98:D3:32:10:ED:3E' # robot-ie-00
# BT_ADDR = '98:D3:31:FB:27:14' # TME-9
# BT_ADDR = '00:25:00:00:28:5C' # IE-35
# BT_ADDR = '00:25:00:00:2C:49' # IE-32
BT_ADDR = '00:25:00:00:32:6A' # IE-38
BT_CHANNEL = 1

if __name__ == '__main__':
    app = Application((512, 512), BT_ADDR, BT_CHANNEL)
    app.run()
