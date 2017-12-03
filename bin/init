#! /usr/bin/env python

import sys
import os

def main():
	"""
	Creates the two files /<init-fname>.bank and /<init-fname>.atm.
	In the specified directory
	
	"""

	print(sys.argv)

	# ensure proper formatting of command
	if len(sys.argv) != 2:
		print("Usage:\tinit <filename>")
		exit(62)
	else:
		# extract path for bank and atm files
		file_pathname = sys.argv[1]
		bank_file_name = file_pathname + '.bank'
		atm_file_name = file_pathname + '.atm'

		# ensure the files haven't been created in that path already
		if os.path.isfile(bank_file_name) or os.path.isfile(atm_file_name):
			print('Error:\tone of the files already exists')
			exit(63)
		
		# attempt to create both the files
		try:
			bank_file = open(bank_file_name, 'w+')
			atm_file = open(atm_file_name, 'w+')

		except:
			# something went wrong with file creation
			print("Error creating initialization files")
			exit(64)

	# both files have been created without error
	bank_file.close()
	atm_file.close()
	print("Successfully initialized bank state")
	return 0


if __name__ == '__main__':
	main()