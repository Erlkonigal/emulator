#!/usr/bin/env python3
"""Generate hardware_config.h from hardware.conf"""

import os
import sys

def parse_config(config_path):
    """Parse hardware.conf file and return dict of config values."""
    config = {}
    with open(config_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if '=' in line:
                key, value = line.split('=', 1)
                config[key.strip()] = value.strip()
    return config

def get_type_suffix(key):
    """Determine C++ type based on key name."""
    if key.endswith('_PORT') or key.endswith('_SIZE') or key.endswith('_LEN'):
        return 'uint16_t' if '_PORT' in key else 'size_t'
    if key.endswith('_NUM_REGISTERS') or key.endswith('_NUM_CSR') or \
       key.endswith('_CYCLE') or key.endswith('_CONSUMPTION') or \
       key.endswith('_QUEUE_SIZE') or key.endswith('_FREQUENCY'):
        return 'uint32_t'
    if key.endswith('_PADDING'):
        return 'size_t'
    return 'uint64_t'

def format_value(value, key):
    """Format value for C++ output."""
    if value.startswith('0x') or value.startswith('0X'):
        return value
    try:
        int(value)
        return value
    except ValueError:
        return value

def generate_header(config, output_path):
    """Generate hardware_config.h from config dict."""
    lines = [
        '#pragma once',
        '',
        '#include <cstddef>',
        '#include <cstdint>',
        '',
    ]
    
    for key, value in sorted(config.items()):
        cpp_type = get_type_suffix(key)
        formatted_value = format_value(value, key)
        name = 'k' + ''.join(word.capitalize() for word in key.split('_'))
        name = name.replace('Num', 'Num')
        
        if cpp_type == 'uint64_t':
            lines.append(f'constexpr {cpp_type} {name} = {formatted_value}ull;')
        elif cpp_type == 'size_t':
            lines.append(f'constexpr {cpp_type} {name} = {formatted_value};')
        else:
            lines.append(f'constexpr {cpp_type} {name} = {formatted_value};')
    
    lines.append('')
    lines.append(f'constexpr const char *kRomDeviceName = "ROM";')
    lines.append(f'constexpr const char *kRamDeviceName = "RAM";')
    lines.append('')
    
    with open(output_path, 'w') as f:
        f.write('\n'.join(lines))

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(script_dir)
    
    config_path = os.path.join(project_dir, 'config', 'hardware.conf')
    output_path = os.path.join(project_dir, 'include', 'emulator', 'generated', 'hardware_config.h')
    
    if not os.path.exists(config_path):
        print(f"Error: Config file not found: {config_path}", file=sys.stderr)
        sys.exit(1)
    
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    
    config = parse_config(config_path)
    generate_header(config, output_path)
    print(f"Generated: {output_path}")

if __name__ == '__main__':
    main()