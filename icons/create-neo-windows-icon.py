#!/usr/bin/env python3
"""Generate the Windows ICO from Neo's centered cyan S mark."""

from pathlib import Path

from PIL import Image, ImageDraw


def cubic(p0, p1, p2, p3, steps=18):
	points = []
	for index in range(1, steps + 1):
		t = index / steps
		u = 1.0 - t
		points.append((
			u ** 3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t ** 3 * p3[0],
			u ** 3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t ** 3 * p3[1],
		))
	return points


def path(start, curves, offset_x=3):
	points = [(start[0] + offset_x, start[1])]
	current = start
	for control1, control2, endpoint in curves:
		points.extend((x + offset_x, y) for x, y in cubic(current, control1, control2, endpoint))
		current = endpoint
	return points


main_mark = path((74, 29), [
	((68, 24), (61, 22), (52, 22)), ((37, 22), (27, 29), (27, 40)),
	((27, 50), (35, 55), (49, 58)), ((58, 60), (62, 63), (62, 68)),
	((62, 74), (56, 77), (48, 77)), ((39, 77), (31, 73), (25, 67)),
	((25, 67), (20, 76), (20, 76)), ((27, 84), (37, 88), (49, 88)),
	((65, 88), (75, 80), (75, 68)), ((75, 56), (67, 51), (53, 48)),
	((44, 46), (40, 43), (40, 39)), ((40, 34), (45, 32), (52, 32)),
	((59, 32), (66, 35), (70, 39)), ((70, 39), (74, 29), (74, 29)),
])

highlight = path((71, 31), [
	((65, 27), (59, 26), (52, 26)), ((39, 26), (31, 31), (31, 40)),
	((31, 48), (38, 52), (50, 55)), ((60, 57), (66, 61), (66, 68)),
	((66, 70), (66, 71), (65, 73)), ((65, 63), (58, 61), (48, 59)),
	((34, 56), (27, 50), (27, 40)), ((27, 29), (37, 22), (52, 22)),
	((61, 22), (68, 24), (74, 29)), ((74, 29), (71, 31), (71, 31)),
])


def render(size=1024):
	scale = size / 108.0
	image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
	draw = ImageDraw.Draw(image)
	draw.rounded_rectangle((0, 0, size - 1, size - 1), radius=24 * scale, fill="#071423")
	draw.polygon([(round(x * scale), round(y * scale)) for x, y in main_mark], fill="#12A8D4")
	draw.polygon([(round(x * scale), round(y * scale)) for x, y in highlight], fill="#65E7F3")
	return image


if __name__ == "__main__":
	output = Path(__file__).resolve().parents[1] / "packaging" / "windows" / "subsurface-neo.ico"
	render().save(output, format="ICO", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
	print(output)
