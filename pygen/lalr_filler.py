import os
import sys
import json
from stat import S_IREAD, S_IRGRP, S_IROTH, S_IWUSR 
from string import Template

if __name__ == '__main__':
	templateFilename = sys.argv[1]
	jsonFilename = sys.argv[2]
	outputFilename = sys.argv[3]

	templateSrc = ''
	with open(templateFilename, 'r') as templateFile:
		templateSrc = templateFile.read()

	templatingMapping = {}
	with open(jsonFilename, 'r') as jsonFile:
		templatingMapping = json.loads(jsonFile.read())

	templateSrc = Template(templateSrc).substitute(templatingMapping)

	if os.path.isfile(outputFilename):
		os.chmod(outputFilename, S_IWUSR|S_IREAD)
	with open(outputFilename, 'w') as outputFile:
		outputFile.write(templateSrc)
	os.chmod(outputFilename, S_IREAD|S_IRGRP|S_IROTH)
