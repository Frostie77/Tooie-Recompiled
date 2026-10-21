"""Constructed extraction controls, not recovered USA ROM contents."""
import importlib.util
from pathlib import Path
import struct
import json
import subprocess
import sys
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("table", Path(__file__).resolve().parents[1] / "tools/inspect_first_person_table.py")
table = importlib.util.module_from_spec(spec)
spec.loader.exec_module(table)


class TableTest(unittest.TestCase):
    def test_signature_bounds_and_all_slots(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/"constructed.bin"
            fixture = json.loads((Path(__file__).parent/'fixtures/bsfirstp_table_us.json').read_text())
            targets = [int(v,16) for v in fixture['raw_offsets']]
            with path.open("wb") as file:
                file.seek(table.ROM_BASE+0x59EC)
                file.write(struct.pack(">5I", *table.DISPATCH))
                file.seek(table.ROM_BASE+table.TABLE_OFFSET)
                file.write(struct.pack(">47I", *targets))
            original = path.read_bytes()
            result = table.inspect(path)
            self.assertEqual(result["count"],47)
            self.assertEqual(result["raw_offset_words"][-1],"00005AE0")
            self.assertFalse(result['matches_step105_rom'])
            cli = subprocess.run([sys.executable,spec.origin,str(path)],capture_output=True,text=True)
            self.assertEqual(cli.returncode,1)
            self.assertIn('complete decompressed ROM size/SHA-256 differs',cli.stderr)
            self.assertEqual(path.read_bytes(),original)
            for offset, value in [(0x59EC,0), (table.TABLE_OFFSET,0x5AE1),
                                  (table.TABLE_OFFSET,table.FUNCTION_START-4),
                                  (table.TABLE_OFFSET,table.FUNCTION_END),
                                  (table.TABLE_OFFSET,0x5A20),
                                  (table.TABLE_OFFSET+46*4,0x5A20)]:
                with self.subTest(offset=offset,value=value):
                    path.write_bytes(original)
                    with path.open("r+b") as file:
                        file.seek(table.ROM_BASE+offset);file.write(struct.pack(">I",value))
                    with self.assertRaises(ValueError):table.inspect(path)
            path.write_bytes(original[:-1])
            with self.assertRaises(ValueError):table.inspect(path)
            with self.assertRaises(OSError):table.inspect(Path(folder)/"absent")


if __name__ == "__main__":unittest.main()
