import pandas
import matplotlib.pyplot as plt
data = pandas.read_json("bench.json")
levels = ['legacy', 'partial', 'element', 'full']
orders = range(1,9)
devices = ['cpu', 'cuda']
device = 'ceed-cuda'
min_dof = 1e3
max_dof = 1e8
min_mdofs = 1e-3
max_mdofs = 5000
min_mdofs_asm = 1e-3
max_mdofs_asm = 10
min_time = 1e-4
max_time = 100 #30*60
######################
# CG (apply) analysis

plt.figure()
for order in orders:
  case = data.loc[(data['order'] == order) & (data['assembly'] == 'partial') & (data['device'] == device)].sort_values('ndofs')
  plt.semilogx(case['cg_time_per_iter'], case['cg_mdofs_per_sec'], marker='+', label=order)

#:wqplt.ylim( [min_mdofs, max_mdofs] )
#plt.xlim( [min_dof, max_dof] )
plt.grid(True)
plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left', borderaxespad=0.)

## Show the plots...
#plt.show()
#import matplotlib2tikz

#matplotlib2tikz.save("jed.tex")
import tikzplotlib

tikzplotlib.save("jed.tex")
