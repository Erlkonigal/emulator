#!/usr/bin/env python3
"""Kconfig-based configuration tool.

Usage:
    python3 scripts/config.py          # Interactive menuconfig
    python3 scripts/config.py --gen    # Generate header only (non-interactive)
"""

import argparse
import os
import sys

try:
    import kconfiglib
    import menuconfig
except ImportError:
    print("Error: kconfiglib not installed.", file=sys.stderr)
    print("Install with: apt install python3-kconfiglib", file=sys.stderr)
    sys.exit(1)


def write_header(kconf, path):
    """Generate hardware_config.h with constexpr format."""
    lines = [
        '#pragma once',
        '',
        '#include <cstddef>',
        '#include <cstdint>',
        '',
    ]
    
    for sym in kconf.unique_defined_syms:
        if sym.visibility == 0:
            continue
        
        name = sym.name
        cpp_name = 'k' + ''.join(word.capitalize() for word in name.split('_'))
        
        if sym.type == kconfiglib.BOOL:
            value = 'true' if sym.str_value == 'y' else 'false'
            lines.append(f'constexpr bool {cpp_name} = {value};')
        elif sym.type == kconfiglib.HEX:
            lines.append(f'constexpr uint64_t {cpp_name} = {sym.str_value}ull;')
        elif sym.type == kconfiglib.INT:
            lines.append(f'constexpr uint32_t {cpp_name} = {sym.str_value};')
    
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        f.write('\n'.join(lines) + '\n')


def main():
    parser = argparse.ArgumentParser(description='Kconfig configuration tool')
    parser.add_argument('--gen', action='store_true',
                        help='Generate header only (non-interactive)')
    args = parser.parse_args()
    
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(script_dir)
    
    kconfig_path = os.path.join(project_dir, 'config', 'Kconfig')
    config_path = os.path.join(project_dir, 'config', '.config')
    header_path = os.path.join(project_dir, 'include', 'emulator', 'generated', 'hardware_config.h')
    
    os.environ['srctree'] = project_dir
    
    kconf = kconfiglib.Kconfig(kconfig_path)
    
    if os.path.exists(config_path):
        kconf.load_config(config_path)
    
    if args.gen:
        write_header(kconf, header_path)
        print(f"Generated: {header_path}")
    else:
        menuconfig.menuconfig(kconf)
        kconf.write_config(config_path)
        write_header(kconf, header_path)
        print(f"Configuration saved to: {config_path}")
        print(f"Generated: {header_path}")


if __name__ == '__main__':
    main()