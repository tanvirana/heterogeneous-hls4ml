import sys

in_width_1 = int(sys.argv[1])
in_width_2 = int(sys.argv[2])
num_entries = 2**(in_width_1 + in_width_2)
result_width = in_width_1 + in_width_2
mul_table = [0 for _ in range(num_entries)]

for i in range(2**in_width_1):
  for j in range(2**in_width_2):
    mul_table[(i << in_width_2) | j] = i * j

data_str = ",".join(str(x) for x in mul_table)

with open("mul_table_{}_{}.h".format(in_width_1, in_width_2), "w") as file:
  file.write("""
#ifndef MUL_{}_{}_H_
#define MUL_{}_{}_H_

#ifndef __SYNTHESIS__
ap_uint<{}> mul_table_{}_{}[{}];
#else
ap_uint<{}> mul_table_{}_{}[{}] = {{ {} }};

#endif

#endif
             """.format(in_width_1, in_width_2,
                        in_width_1, in_width_2,
                        result_width, in_width_1, in_width_2, num_entries, 
                        result_width, in_width_1, in_width_2, num_entries,
                        data_str))
  
with open("mul_table_{}_{}.txt".format(in_width_1, in_width_2), "w") as file:
  file.write(data_str)
