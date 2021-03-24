enum touch_source {
	TOUCH_READ_X1, TOUCH_READ_X2, TOUCH_READ_X3, TOUCH_READ_X4,
	TOUCH_READ_Y1, TOUCH_READ_Y2, TOUCH_READ_Y3, TOUCH_READ_Y4,
	TOUCH_READ_P1, TOUCH_READ_P2,
	TOUCH_READ_RX1, TOUCH_READ_RX2,
	TOUCH_READ_RY1, TOUCH_READ_RY2,
};

void mcpinit(void);
ushort mcpadcread(int ts);
void mcptouchsetup(int ts);
void mcpgpiowrite(ushort mask, ushort data);
void mcpgpiosetdir(ushort mask, ushort dir);
ushort mcpgpioread(void);

