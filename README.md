# render-matrix
Generate a 3D visualization of a matrix.

Read matrices from files, either stdin, or additional command line arguments.
Each line represents a line in the matrix.  Multiple matrices are separated
by an empty line.

Display the matrix in a window and allow some modifications.

Optionally, export the display to a file (pdf, svg, png, tex (tikz)), specifying
rotation and other options on the command line. If multiple matrices are given,
use the same bounding rectangle and zero level. This is useful for animations.
