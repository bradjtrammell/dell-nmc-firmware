"""Pure schema generator validation; tests never use live NMC values."""
import importlib.util
from pathlib import Path
import unittest

GENERATOR = Path(__file__).resolve().parents[1] / 'tools/generate_schema.py'
spec = importlib.util.spec_from_file_location('generator', GENERATOR)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def fixture():
    groups = []
    for g in range(24):
        size = 14 if g < 14 else 10  # 14*14 + 10*10 = 296
        if g == 23:
            size += 6
        groups.append({
            'id': f'Group{g}', 'hiddenInOEM': g == 3,
            'items': [{'name': f'System.Group{g}.Setting{i}', 'access': 'RW',
                       'type': 'Integer:0..100', 'unit': 's',
                       'requiresRestart': False} for i in range(size)]
        })
    return {'schemaVersion': 1, 'configurationGroups': groups,
            'counts': {'configurationItems': 302}}


class GeneratorTests(unittest.TestCase):
    def test_compiles_302_metadata_fields(self):
        src = module.compile_contract(fixture())
        self.assertEqual(src.count('    {"Group'), 302)
        self.assertIn('const ou_schema_field ou_schema_fields[]', src)
        self.assertNotIn('defaultValue', src)

    def test_duplicate_field_rejected(self):
        data = fixture()
        data['configurationGroups'][1]['items'][0]['name'] = data['configurationGroups'][0]['items'][0]['name']
        with self.assertRaises(ValueError):
            module.compile_contract(data)

    def test_invalid_type_token_rejected(self):
        data = fixture()
        data['configurationGroups'][0]['items'][0]['type'] = 'String:4</script>'
        with self.assertRaises(ValueError):
            module.compile_contract(data)

    def test_invalid_access_rejected(self):
        data = fixture()
        data['configurationGroups'][0]['items'][0]['access'] = 'ROOT'
        with self.assertRaises(ValueError):
            module.compile_contract(data)


if __name__ == '__main__':
    unittest.main()
