import Image, ImageDraw, sys
from math import log,exp

"""
Python script to visualize subdivision.

Data format:
1 -0.866025 0.5 0 -0.5 0.866025 0 0.866025 0.5 0 1 -5.58986e-08 0 1.41421
iteration_number [intvall_one] [interval_two] segdistance
"""


#def circle_curve(t, r=1.0):dddd
#   t = 2 * math.pi * t;
#   return (vector( [r*sin(t),r*cos(t), 0]), vector([cos(t), -sin(t),0]))

def normalize(x):
  return x*90+100

def main_cmd(argv):
   i=0
   line = sys.stdin.readline()
   while line:
     im = Image.new("RGB",(200,200),color="white")
     draw = ImageDraw.Draw(im)
     draw.ellipse((10,10,190,190),outline="red")
     (it, x00, y00, z00, x01, y01, z01, x10, y10, z10, x11, y11, z11,d) =  map(float,line.split())
     (x00, y00, z00, x01, y01, z01, x10, y10, z10, x11, y11, z11) = map(normalize, (x00, y00, z00, x01, y01, z01, x10, y10, z10, x11, y11, z11))
     #print it,  x00, y00, x01, y01, x10, y10, x11, y11, d
     draw.line([(x00, y00), (x01, y01)], fill="black")
     draw.line([(x10, y10), (x11, y11)], fill="black")
     draw.text((70,90), "d=%f" % d ,fill="black")
     #im.show()
     im.save("xxx-%03d-%03d.png" % (it, i))
     print "xxx-%03d-%03d.png" % (it, i)
     line = sys.stdin.readline()
     i += 1
   print """Do: mencoder "mf://xxx-*.png" -mf fps=1 -o xxx.avi -ovc copy"""

if __name__=="__main__":
   import sys
   main_cmd(sys.argv)

