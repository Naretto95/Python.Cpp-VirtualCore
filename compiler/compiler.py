import m_parser as ps
import interpreter as inter
import sys

inter.interpreter(ps.parser(sys.argv[1]))
