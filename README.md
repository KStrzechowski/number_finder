NumberFinder
Program finds numbers from a given range in files under specified directory. Program creates index file with offsets of found numbers.

How to run
make
./numf [flags]

Flags
-r - recursive searching in directories
-m - lower limit of searching numbers (default = 10)
-M - upper limit of searching numbers (default = 1000)
-i - after time specified by -i indexing will restart (default = 600)

Commands
querry [n1] [n2] ... - returns offsets of specified numbers
status - returns information if indexing thread is running
index - starts indexing thread
exit / SIGINT - ends program