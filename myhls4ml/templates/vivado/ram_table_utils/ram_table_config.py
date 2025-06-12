import re
from generate_add_table_1d import generate_add_table
from generate_mul_table_1d import generate_mul_table
import math

def build_ram_add_tables_h(
        add_tables_width, 
        add_tables_dual_port, 
        add_tables_lut_ram, 
        add_tables_ram_partition_factor
    ):

    includes = ""
    for width in add_tables_width:
        includes += '#include "add_table_{}.h"\n'.format(width)

    fallback = """

template <int width, bool dual_port, bool lut_ram, int partition_factor>
ap_uint<width>* ram_add_tables() {{
    // fallback: no table for these widths
    #ifndef __SYNTHESIS__
        printf("Undefined bram add call");
    #endif
    return nullptr;
}}
    """

    template_instances = ""

    for i in range(len(add_tables_width)):

        pragmas = ""
        if add_tables_lut_ram[i]:
            pragmas = """
#pragma HLS RESOURCE variable=add_table_{} core=ROM_{}P_LUTRAM
#pragma HLS ARRAY_PARTITION variable=add_table_{} complete
""".format(add_tables_width[i], 
           "2" if add_tables_dual_port[i] else "1", 
           add_tables_width[i])
        else:
            pragmas = """
#pragma HLS RESOURCE variable=add_table_{} core=ROM_{}P_BRAM
#pragma HLS ARRAY_PARTITION variable=add_table_{} cyclic factor={}
""".format(add_tables_width[i], 
           "2" if add_tables_dual_port[i] else "1", 
           add_tables_width[i],
           add_tables_ram_partition_factor[i])

        template_instances += """
template <>
ap_uint<{}>* ram_add_tables<{},{},{},{}>() {{
#ifndef __SYNTHESIS__
    static bool loaded_weights = false;
    if (!loaded_weights) {{
        printf("Loading add table {}");
        nnet::load_weights_from_txt<ap_uint<{}>,{}>(add_table_{}, "../ram_table_utils/add_table_{}.txt");
        loaded_weights = true;    
    }}
    {}
#endif
    return add_table_{};
}}

""".format(add_tables_width[i], 
           add_tables_width[i], 
           "true" if add_tables_dual_port[i] else "false",
           "true" if add_tables_lut_ram[i] else "false",
           add_tables_ram_partition_factor[i],
           add_tables_width[i],
           add_tables_width[i],
           math.pow(2, add_tables_width[i] * 2),
           add_tables_width[i],
           add_tables_width[i],
           pragmas,
           add_tables_width[i]
           )

    with open("test_ram_add_tables.h", "w") as file:
        file.write(includes + fallback + template_instances)

def build_ram_mul_tables_h(
        mul_tables_width1, 
        mul_tables_width2,
        mul_tables_dual_port, 
        mul_tables_lut_ram, 
        mul_tables_ram_partition_factor
    ):

    includes = ""
    for i in range(len(mul_tables_width1)):
        includes += '#include "mul_table_{}_{}.h"\n'.format(mul_tables_width1[i], mul_tables_width2[i])

    fallback = """

template <int width1, int width2, bool dual_port, bool lut_ram, int partition_factor>
ap_uint<width1 + width2>* ram_mul_tables() {{
    // fallback: no table for these widths
    #ifndef __SYNTHESIS__
        printf("Undefined bram add call");
    #endif
    return nullptr;
}}

    """

    template_instances = ""

    for i in range(len(mul_tables_width1)):

        pragmas = ""
        if mul_tables_lut_ram[i]:
            pragmas = """
#pragma HLS RESOURCE variable=mul_table_{}_{} core=ROM_{}P_LUTRAM
#pragma HLS ARRAY_PARTITION variable=mul_table_{}_{} complete
""".format(mul_tables_width1[i], mul_tables_width2[i], 
           "2" if mul_tables_dual_port[i] else "1", 
           mul_tables_width1[i], mul_tables_width2[i])
        else:
            pragmas = """
#pragma HLS RESOURCE variable=mul_table_{}_{} core=ROM_{}P_BRAM
#pragma HLS ARRAY_PARTITION variable=mul_table_{}_{} cyclic factor={}
""".format(mul_tables_width1[i], mul_tables_width2[i], 
           "2" if mul_tables_dual_port[i] else "1", 
           mul_tables_width1[i], mul_tables_width2[i],
           mul_tables_ram_partition_factor[i])

        template_instances += """
template <>
ap_uint<{}>* ram_mul_tables<{},{},{},{},{}>() {{
#ifndef __SYNTHESIS__
    static bool loaded_weights = false;
    if (!loaded_weights) {{
        printf("Loading mul table {} {}");
        nnet::load_weights_from_txt<ap_uint<{}>,{}>(mul_table_{}_{}, "../ram_table_utils/mul_table_{}_{}.txt");
        loaded_weights = true;    
    }}
    {}
#endif
    return mul_table_{}_{};
}}

""".format(mul_tables_width1[i] + mul_tables_width2[i], 
           mul_tables_width1[i], mul_tables_width2[i],
           "true" if mul_tables_dual_port[i] else "false",
           "true" if mul_tables_lut_ram[i] else "false",
           mul_tables_ram_partition_factor[i],
           mul_tables_width1[i], mul_tables_width2[i],
           mul_tables_width1[i] + mul_tables_width2[i],
           math.pow(2, mul_tables_width1[i] + mul_tables_width2[i]),
           mul_tables_width1[i], mul_tables_width2[i],
           mul_tables_width1[i], mul_tables_width2[i],
           pragmas,
           mul_tables_width1[i], mul_tables_width2[i]
           )

    with open("test_ram_mul_tables.h", "w") as file:
        file.write(includes + fallback + template_instances)

def extract_struct_blocks(code):
    lines = code.splitlines()

    func_names = []      # comment above struct, e.g. "softmax"
    struct_names = []    # struct names, e.g. "softmax_config13"
    struct_bodies = []

    i = 0
    while i < len(lines):
        line = lines[i].strip()

        # Check if this line starts with 'struct'
        if line.startswith("struct"):
            # Grab comment immediately above if it exists
            comment_line = lines[i - 1].strip() if i > 0 else ""
            comment = comment_line[2:].strip() if comment_line.startswith("//") else None

            # Extract struct name via regex: after 'struct' up to first space or ':'
            m = re.match(r'struct\s+([^\s:]+)', line)
            struct_name = m.group(1) if m else None

            # Find the opening brace if not on the same line
            while '{' not in lines[i]:
                i += 1
                if i >= len(lines):
                    break

            brace_level = 0
            struct_content = []
            while i < len(lines):
                l = lines[i]
                brace_level += l.count('{')
                brace_level -= l.count('}')
                struct_content.append(l)
                if brace_level == 0:
                    break
                i += 1

            # Save extracted info if comment exists
            if comment is not None:
                func_names.append(comment)
                struct_names.append(struct_name)
                struct_bodies.append('\n'.join(struct_content))

        i += 1

    return func_names, struct_names, struct_bodies

def _line_contains_pattern(line, pattern):
    """Check if the line contains pattern before any comment '//'"""
    comment_pos = line.find("//")
    pattern_pos = line.find(pattern)
    if pattern_pos == -1:
        return False
    # pattern found, now check if comment appears before it
    if comment_pos != -1 and comment_pos < pattern_pos:
        return False
    return True

def find_struct_indexes_with_ram_muls(struct_bodies):
    matching_indexes = []

    for idx, body in enumerate(struct_bodies):
        lines = body.splitlines()
        for line in lines:
            line = line.strip()
            if line.startswith("//"):
                continue
            if _line_contains_pattern(line, 'ram_muls'):
                # Extract number safely ignoring comments
                match = re.search(r'ram_muls\s*=\s*(\d+)\s*;', line)
                if match and int(match.group(1)) > 0:
                    matching_indexes.append(idx)
                    break
    return matching_indexes

def find_struct_indexes_with_ram_add(struct_bodies):
    matching_indexes = []

    for idx, body in enumerate(struct_bodies):
        lines = body.splitlines()
        for line in lines:
            line = line.strip()
            if line.startswith("//"):
                continue
            if _line_contains_pattern(line, 'ram_add'):
                if re.search(r'ram_add\s*=\s*true\s*;', line):
                    matching_indexes.append(idx)
                    break
    return matching_indexes

def has_dual_port_muls(struct_str):
    lines = struct_str.splitlines()
    for line in lines:
        line = line.strip()
        if line.startswith("//"):
            continue
        if _line_contains_pattern(line, 'dual_port_muls'):
            if re.search(r'dual_port_muls\s*=\s*true\s*;', line):
                return True
    return False

def has_dual_port_add(struct_str):
    lines = struct_str.splitlines()
    for line in lines:
        line = line.strip()
        if line.startswith("//"):
            continue
        if _line_contains_pattern(line, 'dual_port_add'):
            if re.search(r'dual_port_add\s*=\s*true\s*;', line):
                return True
    return False

def has_lut_ram_muls(struct_str):
    lines = struct_str.splitlines()
    for line in lines:
        line = line.strip()
        if line.startswith("//"):
            continue
        if _line_contains_pattern(line, 'lut_ram_muls'):
            if re.search(r'lut_ram_muls\s*=\s*true\s*;', line):
                return True
    return False

def has_lut_ram_add(struct_str):
    lines = struct_str.splitlines()
    for line in lines:
        line = line.strip()
        if line.startswith("//"):
            continue
        if _line_contains_pattern(line, 'lut_ram_add'):
            if re.search(r'lut_ram_add\s*=\s*true\s*;', line):
                return True
    return False

def get_ram_partition_factor_muls(struct_str):
    lines = struct_str.splitlines()
    for line in lines:
        line = line.strip()
        if line.startswith("//"):
            continue
        if _line_contains_pattern(line, 'ram_partition_factor_muls'):
            match = re.search(r'ram_partition_factor_muls\s*=\s*(\d+)\s*;', line)
            if match:
                return int(match.group(1))
    return 1

def get_ram_partition_factor_add(struct_str):
    lines = struct_str.splitlines()
    for line in lines:
        line = line.strip()
        if line.startswith("//"):
            continue
        if _line_contains_pattern(line, 'ram_partition_factor_add'):
            match = re.search(r'ram_partition_factor_add\s*=\s*(\d+)\s*;', line)
            if match:
                return int(match.group(1))
    return 1

def get_first_template_param(code, config_name):
    """
    Given code and a config struct name (e.g. 'relu_config4'),
    returns the first template parameter from the matching nnet layer call.
    """

    # Regex to find lines like:
    # nnet::<layer><<type1, type2, config_name>>(...
    # This matches template parameters inside <>
    pattern = re.compile(
        r'nnet::\w+<\s*([^,>]+)\s*,\s*([^,>]+)\s*,\s*(' + re.escape(config_name) + r')\s*>'
    )

    matches = pattern.findall(code)

    if matches:
        # matches is a list of tuples like [(type1, type2, config_name), ...]
        # We want the first type1 from the first match
        return matches[0][0].strip()
    else:
        return None

def get_bit_width(type_name, typedef_str):
    # Regex to find typedef for the type_name
    # It matches lines like: typedef ap_fixed<16,6> input_t;
    pattern = rf'typedef\s+ap_(?:fixed|ufixed|uint|int)<\s*(\d+),.*>\s+{re.escape(type_name)};'
    
    match = re.search(pattern, typedef_str)
    if match:
        return int(match.group(1))
    else:
        return None  # not found
    
def get_weight_t_type(struct_body):
    lines = struct_body.splitlines()
    for line in lines:
        line = line.strip()
        # Skip comments
        if line.startswith("//"):
            continue
        # Check if weight_t typedef exists in line (and not commented inline)
        # Only consider part before any '//' comment on the line
        code_part = line.split("//")[0].strip()
        match = re.match(r'typedef\s+([a-zA-Z0-9_]+)\s+weight_t\s*;', code_part)
        if match:
            return match.group(1)
    return None

def get_accum_t_type(struct_body):
    lines = struct_body.splitlines()
    for line in lines:
        line = line.strip()
        # Skip comments
        if line.startswith("//"):
            continue
        # Ignore inline comments by splitting on '//' and taking the code before that
        code_part = line.split("//")[0].strip()
        match = re.match(r'typedef\s+([a-zA-Z0-9_]+)\s+accum_t\s*;', code_part)
        if match:
            return match.group(1)
    return None

with open("../parameters.h", "r") as file:
    parameter_file = file.read()

layer_name, config_names, config_string = extract_struct_blocks(parameter_file)
ram_mul_idx = find_struct_indexes_with_ram_muls(config_string)

mul_tables = []
mul_tables_dual_port = []
mul_tables_lut_ram = []
mul_tables_ram_partition_factor = []

for idx in ram_mul_idx:
    mul_tables_dual_port.append(has_dual_port_muls(config_string[idx]))
    mul_tables_lut_ram.append(has_lut_ram_muls(config_string[idx]))
    mul_tables_ram_partition_factor.append(get_ram_partition_factor_muls(config_string[idx]))

ram_add_idx = find_struct_indexes_with_ram_add(config_string)

add_tables_dual_port = []
add_tables_lut_ram = []
add_tables_ram_partition_factor = []

for idx in ram_add_idx:
    add_tables_dual_port.append(has_dual_port_muls(config_string[idx]))
    add_tables_lut_ram.append(has_lut_ram_muls(config_string[idx]))
    add_tables_ram_partition_factor.append(get_ram_partition_factor_muls(config_string[idx]))


with open("../myproject.cpp", "r") as file:
    project_file = file.read()


with open("../defines.h", "r") as file:
    defines_file = file.read()

mul_tables_width1 = []
mul_tables_width2 = []

for idx in ram_mul_idx:
    
    input_type = get_first_template_param(project_file, config_names[idx])
    weight_type = get_weight_t_type(config_string[idx])
    mul_tables_width1.append(get_bit_width(input_type, defines_file))
    mul_tables_width2.append(get_bit_width(weight_type, defines_file))

add_tables_width = []

for idx in ram_add_idx:

    accum_type = get_accum_t_type(config_string[idx])
    add_tables_width.append(get_bit_width(accum_type, defines_file))

for i in range(len(ram_mul_idx)):
    generate_mul_table(mul_tables_width1[i], mul_tables_width2[i])

for i in range(len(ram_add_idx)):
    generate_add_table(add_tables_width[i])

build_ram_mul_tables_h(mul_tables_width1, mul_tables_width2,
                       mul_tables_dual_port,
                       mul_tables_lut_ram,
                       mul_tables_ram_partition_factor)
build_ram_add_tables_h(add_tables_width, 
                      add_tables_dual_port, 
                      add_tables_lut_ram, 
                      add_tables_ram_partition_factor)