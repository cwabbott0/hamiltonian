#!/usr/bin/python

import sys
import math
import argparse
import subprocess
import Image, ImageDraw

def num_edges(matrix):
	ret = 0
	for i, list in enumerate(matrix):
		for value in list[:i]:
			if value:
				ret += 1
	return ret

def to_metis_format(matrix, out_name):
	out = open(out_name, 'w')
	out.write("{} {}\n".format(len(matrix), num_edges(matrix)))
	for adj in matrix:
		for j, value in enumerate(adj):
			if value:
				out.write("{} ".format(j + 1))
		out.write("\n")
	out.close()

#returns the name of the file with the partition data, used to constrain hamilton cycles
def run_metis(matrix):
	temp_name = "temp.graph"
	to_metis_format(matrix, temp_name)
	nparts = int(math.sqrt(len(matrix)))
	out_name = "temp.graph.part." + str(nparts)
	subprocess.check_call(["gpmetis", temp_name, str(nparts)])
	return out_name

def get_hamilton_cycle(filename):
	return [int(elem) for elem in subprocess.check_output(["./hamiltonian", filename]).rstrip(' \n').split(' ')]

def parse_adj_matrix(file):
	return [[char == '1' for char in line.rstrip('\n').split(' ')] for line in file]

def permute_adj_matrix(matrix, cycle):
	return [[matrix[cycle[i]][cycle[j]] for j in range(len(cycle))] for i in range(len(cycle))]

circle_radius = 10.0
circle_spacing = 5.0 * circle_radius # the distance between the circle centers

def circle_pos(index, n):
	angle = float(index) / n * math.pi * 2
	return (math.sin(angle) * circle_distance + width / 2.0, math.cos(angle) * circle_distance + height / 2.0)

def line_pos(index1, index2, n):
	x1, y1 = circle_pos(index1, n)
	x2, y2 = circle_pos(index2, n)
	norm = 1.0 / math.sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2))
	off_x = (x2 - x1) * norm * circle_radius
	off_y = (y2 - y1) * norm * circle_radius
	return (x1 + off_x, y1 + off_y, x2 - off_x, y2 - off_y)

def draw_circles(draw, n):
	for i in range(n):
		x, y = circle_pos(i, n)
		draw.ellipse((x - circle_radius, y - circle_radius, x + circle_radius, y + circle_radius), fill="white", outline="black")

def draw_lines(draw, matrix):
	for i in range(len(matrix)):
		for j in range(i):
			if matrix[i][j]:
				draw.line(line_pos(i, j, len(matrix)), fill="black")

def set_size(n):
	global circle_distance, width, height
	half_angle = math.pi / n
	circle_distance = (circle_spacing / 2.0) / math.sin(half_angle)
	width = height = int(math.ceil(2 * circle_radius + 2 * circle_distance))

def draw_image(matrix):
	set_size(len(matrix))
	image = Image.new("RGB", (width, height), "white")
	draw = ImageDraw.Draw(image)
	draw_circles(draw, len(matrix))
	draw_lines(draw, matrix)
	return image

parser = argparse.ArgumentParser()
parser.add_argument("input", help="input adjaceny matrix file")
parser.add_argument("-r", "--radius", type=float, default=10.0, help="circle radius")
parser.add_argument("-s", "--spacing", type=float, default=5.0,
                    help="distance between the center of each circle in terms of the radius")
parser.add_argument("-o", "--output", help="output image file")
args = parser.parse_args()
circle_radius = args.radius
circle_spacing = args.spacing * circle_radius

try:
	file = open(args.input, 'r')
	adj_matrix = parse_adj_matrix(file)
	#print run_metis(adj_matrix)
	cycle = get_hamilton_cycle(args.input)
	adj_matrix = permute_adj_matrix(adj_matrix, cycle)
	image = draw_image(adj_matrix)
	image.save(args.output)
except IOError:
	print 'Error: could not open file ' + args.input
	