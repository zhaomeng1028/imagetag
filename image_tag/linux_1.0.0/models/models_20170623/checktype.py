for line in open('threshold.txt','r'):
    line = line.strip().split('\t')
    print line[0]
    print type(line[0])
    print line[1]
    print type(line[1])
