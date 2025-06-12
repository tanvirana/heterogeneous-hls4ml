import os

script_dir = os.path.dirname(os.path.abspath(__file__))  # directory of the current script

def generate_add_table(in_width):
  num_entries = 2**(2*in_width)
  dim_size = 2**in_width
  add_table = [0 for _ in range(num_entries)]

  for i in range(dim_size):
    for j in range(dim_size):
      add_table[(i << in_width) | j] = min(i + j, 2**in_width - 1)

  data_str = ",".join(str(x) for x in add_table)

  add_table_filepath = os.path.join(script_dir, "add_table_{}.h".format(in_width))

  with open(add_table_filepath, "w") as file:
    file.write("""
  #ifndef ADD_{}_H_
  #define ADD_{}_H_

  #ifndef __SYNTHESIS__
  ap_uint<{}> add_table_{}[{}];
  #else
  ap_uint<{}> add_table_{}[{}] = {{ {} }};

  #endif

  #endif
              """.format(in_width,
                          in_width, 
                          in_width, in_width, num_entries, 
                          in_width, in_width, num_entries,
                          data_str))
    
  with open("add_table_{}.txt".format(in_width), "w") as file:
    file.write(data_str)