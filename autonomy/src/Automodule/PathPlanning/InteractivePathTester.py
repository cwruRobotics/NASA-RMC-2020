from graphics import *
import collections
from collections import deque
from PathPlanning.ThetaStar import create_path
from PathPlanning.PathPlanning import Position
from PathFollowing import config
from PathFollowing.PathFollower import PathFollower
from PathFollowing.SkidSteerSimulator import SkidSteerSimulator
import numpy as np



class InteractivePathTester:
    SCALE = 75  # px/meter
    SIM_DELAY = 10  # iterations

    def __init__(self, start, end, arena_width, arena_height, obstacles):
        self.positions = [start, end]
        self.arena_width = arena_width
        self.arena_height = arena_height
        self.obstacles = obstacles
        self.path = None
        self.robot = SkidSteerSimulator(2, 0, 1.5)

        self.controller = PathFollower(self.path, self.robot.reference_point)

        self.win = GraphWin("Path", self.arena_width*self.SCALE, self.arena_height*self.SCALE, autoflush=False)

        self.win.bind("<ButtonPress-1>", self.on_click)
        self.win.bind("<B1-Motion>", self.update_drag_position)
        self.win.bind("<ButtonRelease-1>", self.update_drag_position)

        self.object_id = -1

        self.robot_drawings = []

        self.update_path_and_draw()
        self.simulate_robot()
        update(1000)
        self.win.getMouse()


    def on_click(self, event):
        self.object_id = -1

        x, y = event.widget.winfo_pointerxy()
        x, y = (x-self.win.winfo_rootx()) / self.SCALE, (y-self.win.winfo_rooty()) / self.SCALE
        x, y = x, self.arena_height - y

        for i, obstacle in enumerate(self.obstacles):
            if Position(x, y).distanceTo(Position(obstacle.center_x, obstacle.center_y)) < obstacle.getRadius():
                self.object_id = i
                break
        for i, position in enumerate(self.positions):
            if Position(x, y).distanceTo(position) < 0.1:
                self.object_id = i + len(self.obstacles)
                break

    def update_drag_position(self, event):
        x, y = event.widget.winfo_pointerxy()
        x, y = (x - self.win.winfo_rootx()) / self.SCALE, (y - self.win.winfo_rooty()) / self.SCALE
        x, y = x, self.arena_height - y

        if 0 <= self.object_id < len(self.obstacles):
            self.obstacles[self.object_id].setCenter(x, y)
            self.update_path_and_draw()
        elif self.object_id > 0:
            self.positions[self.object_id - len(self.obstacles)] = Position(x, y)
            self.update_path_and_draw()

    def update_path_and_draw(self):
        # t = time.time_ns()
        self.path, grid = create_path(Position(self.robot.state[0], self.robot.state[1]), self.positions[1], self.arena_width, self.arena_height, self.obstacles)
        # print((time.time_ns()-t)*10**-9)

        path = []
        for p in self.path.path:
            path.append([p.x_pos, p.y_pos])
        path = np.array(path)
        self.controller.set_path(path)

        self.drawPath(self.win, self.path, self.obstacles, self.positions, grid)

    def drawPath(self, win, path, obstacles, positions, grid):
        for item in win.items[:]:
            item.undraw()
        lastX = 0
        lastY = 0
        lines = deque()
        points = deque()
        for i in range(len(path)):
            p = path.path[i]
            if lastX != 0 or lastY != 0:
                l = Line(Point(lastX*self.SCALE, (self.arena_height-lastY)*self.SCALE),
                         Point(p.getX()*self.SCALE, (self.arena_height-p.getY())*self.SCALE))
                lines.append(l)
            c = Circle(Point(p.getX()*self.SCALE, (self.arena_height-p.getY())*self.SCALE), 0.06*self.SCALE)
            c.setFill("blue")
            c.setOutline("blue")
            points.append(c)
            lastX = p.getX()
            lastY = p.getY()
        for obstacle in obstacles:
            c = Circle(Point(obstacle.center_x*self.SCALE, (self.arena_height-obstacle.center_y)*self.SCALE), obstacle.getRadius()*self.SCALE)
            c.setFill("red")
            c.setOutline("red")
            points.append(c)
        for x in lines:
            x.draw(win)
        for x in points:
            x.draw(win)
        for r in range(grid.num_rows):
            for c in range(grid.num_cols):
                if grid.get_prob_blocked(r, c) > 0.2:
                    ci = Circle(Point(((c+0.5) * grid.unit_width) * self.SCALE,
                                      (self.arena_height-(r+0.5) * grid.unit_height) * self.SCALE),
                                      grid.get_prob_blocked(r, c) * 0.08 * self.SCALE)
                    ci.setFill("green")
                    ci.setOutline("green")
                    ci.draw(win)

    def simulate_robot(self):
        dt = 0.01

        for i in range(50):
            target_vel, target_angular_vel = self.controller.get_wheel_torques(self.robot.state, self.robot.state_dot, dt)

            if self.robot.state_dot[0, 0] < target_vel:
                forward_torque = 30
            else:
                forward_torque = 29

            turn_torque = 7 * (target_angular_vel - self.robot.state_dot[2, 0])

            right_torque = forward_torque + turn_torque
            left_torque = forward_torque - turn_torque

            self.robot.update(right_torque, left_torque, dt)

        self.update_path_and_draw()
        self.draw_robot(self.robot, self.controller, self.win)

        self.win.after(int(dt * 1000 * self.SIM_DELAY), self.simulate_robot)

    def draw_robot(self, robot, controller, win):
        points, _, _ = robot.draw()
        robot_path, _, _ = controller.draw_path_info()

        to_draw = []
        for point in points:
            c = Circle(Point(point[0]*self.SCALE, (self.arena_height-point[1])*self.SCALE), 0.05*self.SCALE)
            c.setFill("blue")
            c.setOutline("blue")
            to_draw.append(c)

        lastX = 0
        lastY = 0
        for p in robot_path:
            if lastX != 0 or lastY != 0:
                l = Line(Point(lastX * self.SCALE, (self.arena_height - lastY) * self.SCALE),
                         Point(p[0] * self.SCALE, (self.arena_height - p[1]) * self.SCALE))
                to_draw.append(l)
            lastX = p[0]
            lastY = p[1]

        for item in self.robot_drawings:
            item.undraw()

        for x in to_draw:
            x.draw(win)

        self.robot_drawings = to_draw
