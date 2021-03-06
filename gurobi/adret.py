import sys
import math

#from log import Logger
from logging import Logger
from gurobipy import *

def datareader(log, dataname):
  #log.jointoutput("reading datafile " + dataname + "\n")
  logging.jointoutput("reading datafile " + dataname + "\n")
  N = 0
  M = 0
  try:
    f = open(dataname,"r")
    lines = f.readlines();
    f.close()
  except IOError as (errno,strerror):
    log.jointoutput("cannot open data file " + dataname + "\n")
    log.stateandquit("failure")

  thisline = lines[0].split()
  if len(thisline) != 6:
    log.stateandquit("illegal file structure; first line MUST be of the form n xxx gamma xxx Gamma xxx\n")

  n = int(thisline[1])
  gamma = float(thisline[3])
  Gamma = float(thisline[5])

  log.jointoutput("n = " + str(n) + " gamma = " + str(gamma) + " Gamma = " + str(Gamma) + "\n")
  if n <= 0 or gamma <= 0 or Gamma <= 0:
      log.stateandquit("illegal inputs")

  x = [0]*n
  mu = [0]*n

  thisline = lines[1].split()
  if len(thisline) != n+1:
      log.stateandquit("illegal line for x")

  sumx = 0
  for i in xrange(1, n+1):
      x[i-1] = float(thisline[i])

  sumx = sum(x)
  if math.fabs(sumx - 1.0) > 1e-04:
      log.stateandquit("illegal sum(x) = " + str(sumx))

  #now read mu
  thisline = lines[2].split()
  if len(thisline) != n+1:
      log.stateandquit("illegal line for x")

  for i in xrange(1, n+1):
      mu[i-1] = float(thisline[i])

  return n, gamma, Gamma, x, mu

def formulator(log, n, gamma, Gamma, x, mu, lpfilename):
    log.jointoutput("now formulating\n")

    m = Model("harry")

    delta = {}
    for j in xrange(n):
        delta[j] = m.addVar(obj = x[j], ub = gamma, name = "delta"+str(j))

    # Update model to integrate new variables
    m.update()

    m.modelSense = GRB.MAXIMIZE
    
    #now add the global constraint
    m.addConstr( quicksum(delta[j] for j in xrange(n)) <= Gamma)

    m.update()

    #optionally, write
    m.write(lpfilename)

    # Solve
    m.optimize()

    log.jointoutput("j x[j]   mu[j]  delta[j]  mu[j] - delta[j]\n")
    for j in xrange(n):
        log.jointoutput(str(j) + " " + (str(x[j])).ljust(5,' ') + "  " + (str(mu[j])).ljust(5,' ') + "    " + (str(delta[j].x)).ljust(8,' ') + " " + str(mu[j]-delta[j].x) + "\n")

    nominal = 0
    adversarial = 0
    for j in xrange(n):
        nominal += mu[j]*x[j]
        adversarial += (mu[j] - delta[j].x)*x[j]

    log.jointoutput("nominal return: " + str(nominal)+"; adversarial: " + str(adversarial) + "\n     objective: " + str(m.objval) + ";  net:" + str(nominal - m.objval) + "\n")

###main.  Syntax: adret.py datafile lpfilename.  The data file has the positions, the 
###       expected returns and the risk model (gamma and Gamma)

if len(sys.argv) != 3:  # the program name and datafile
  # stop the program and print an error message                                 
  sys.exit("usage: adret.py dataname lpname ")

log = Logger("adret.log")

n, gamma, Gamma, x, mu = datareader(log,sys.argv[1])

formulator(log, n, gamma, Gamma, x, mu, sys.argv[2])

log.closelog()
