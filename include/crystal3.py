class RGBController:
    def __init__(self, led, colors, timings):
        self.led = led
        self.colors = colors
        self.timings = timings
        self.status = 0
        self.color_index = 0
        self.timer = 0

    def update(self):
        if self.status == 0:
            self.status = 1
            self.color_index = 0
            self.led.color = self.colors[self.color_index]

        elif self.status == 1:
            if self.timer == self.timings[self.color_index]:
                self.timer = 0
                self.color_index += 1
                if self.color_index < len(self.colors):
                    self.led.color = self.colors[self.color_index]
                else:
                    self.status = 2

        elif self.status == 2:
            if self.timer == self.timings[self.color_index]:
                self.timer = 0
                self.color_index -= 1
                if self.color_index >= 0:
                    self.led.color = self.colors[self.color_index]
                else:
                    self.status = 0

        self.timer += 1


# Example usage
rgb_bar0 = RGBLED(red=0, green=1, blue=2)
controller = RGBController(rgb_bar0, Crystal_color_0, Crystal_light_time_0)

while True:
    controller.update()