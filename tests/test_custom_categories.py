"""Native catalog tests; only temporary synthetic packs, no game connection."""
from pathlib import Path
import json,os,shutil,struct,subprocess,tempfile,unittest,zlib


def png():
    def chunk(kind,data):
        return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',2,2,8,6,0,0,0))+chunk(b'IDAT',zlib.compress((b'\0'+bytes((20,180,80,255))*2)*2))+chunk(b'IEND',b'')


@unittest.skipUnless(os.name=='nt' and os.environ.get('TOYPAD_TEST_EXE'),'Set TOYPAD_TEST_EXE to a Windows source build')
class Categories(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory(dir=os.environ.get('TOYPAD_TEST_TEMP'))
        self.addCleanup(self.temporary.cleanup)
        self.root=Path(self.temporary.name)
        self.exe=self.root/'LegoToypad.exe'
        shutil.copy2(os.environ['TOYPAD_TEST_EXE'],self.exe)
        self.original=self.catalog()

    def catalog(self):
        output=self.root/'catalog.json';output.unlink(missing_ok=True)
        subprocess.run([str(self.exe),'--workshop-catalog',str(output)],cwd=self.root,
            check=True,timeout=20,creationflags=subprocess.CREATE_NO_WINDOW)
        self.assertFalse((self.root/'LegoToypad.ini').exists(),'Read-only export wrote app settings')
        return json.loads(output.read_text(encoding='utf-8'))

    def pack(self,folder,name='Example Universe',hero='Example Hero'):
        path=self.root/'Categories'/folder;path.mkdir(parents=True)
        (path/'category.ini').write_text('[Category]\nSchema=1\nName='+name+'\n',encoding='utf-8')
        # Opaque synthetic 180-byte input exercises catalog discovery only.
        # It is not an authentic character tag and is never sent to a game.
        (path/(hero+'.bin')).write_bytes(bytes(180))
        return path

    def worlds(self,catalog):return {w['name']:w for w in catalog['franchises']}

    def test_unicode_encodings_and_invalid_metadata(self):
        path=self.pack('Encoding');title='Étoiles 星';text='[Category]\nSchema=1\nName='+title+'\n'
        valid=[text.encode(e) for e in ('utf-8','utf-8-sig','utf-16')]+[b'\xfe\xff'+text.encode('utf-16-be')]
        invalid=[text.encode()+b'Name=Duplicate\n',b'[Category]\nSchema=77\nName=Rejected\n',
            b'[Category]\nSchema=1\nName=Bad\xff\n',b'\xff\xfeX',b';'*16385,
            b'[Category]\nSchema=1\nName='+b'A'*65,b'[Category]\nSchema=1\nName=Bad\0Name\n']
        for metadata,expected in [(v,True) for v in valid]+[(v,False) for v in invalid]:
            with self.subTest(metadata=metadata[:32]):
                (path/'category.ini').write_bytes(metadata)
                worlds=self.worlds(self.catalog())
                self.assertEqual(title in worlds,expected)
                self.assertEqual(len(worlds),len(self.original['franchises'])+int(expected))

    def test_merged_categories_duplicate_labels_and_artwork(self):
        first=self.pack('A',hero='First Hero');self.pack('B',hero='Second Hero');self.pack('C',hero='First Hero')
        (first/'logo.png').write_bytes(png());(first/'First Hero.png').write_bytes(png())
        worlds=self.worlds(self.catalog());category=worlds['Example Universe']
        self.assertEqual([c['name'] for c in category['characters']],['First Hero','Second Hero'])
        self.assertTrue(category['logo'].startswith('/img/-'))
        self.assertTrue(category['characters'][0]['portrait'].startswith('/img/-'))
        for name,original in self.worlds(self.original).items():self.assertEqual(worlds[name],original)

    def test_stock_art_is_preserved_when_a_pack_extends_its_roster(self):
        stock=self.original['franchises'][0];path=self.pack('StockExtension',name=stock['name'])
        (path/'logo.png').write_bytes(png())
        current=self.worlds(self.catalog())[stock['name']]
        self.assertEqual(current['logo'],stock['logo'])
        self.assertEqual(current['characters'][:-1],stock['characters'])
        self.assertEqual(current['characters'][-1]['name'],'Example Hero')

    def test_discovery_removal_and_legacy_special_folders(self):
        path=self.pack('Temporary');legacy=self.root/'Special'/'Legacy';legacy.mkdir(parents=True)
        (legacy/'Legacy Hero.bin').write_bytes(bytes(180));(legacy/'Invalid.bin').write_bytes(b'wrong size')
        current=self.catalog();self.assertIn('Example Universe',self.worlds(current))
        self.assertEqual([c['name'] for c in current['custom']['characters']],['Legacy Hero'])
        (path/'Example Hero.bin').unlink();current=self.catalog()
        self.assertNotIn('Example Universe',self.worlds(current))
        self.assertEqual([c['name'] for c in current['custom']['characters']],['Legacy Hero'])

    def test_redirected_pack_is_not_followed(self):
        outside=self.root/'outside';outside.mkdir()
        (outside/'category.ini').write_text('[Category]\nSchema=1\nName=Must Not Load\n')
        (outside/'Example.bin').write_bytes(bytes(180))
        categories=self.root/'Categories';categories.mkdir()
        try:os.symlink(outside,categories/'Redirect',target_is_directory=True)
        except OSError as error:self.skipTest('Symlink creation unavailable: '+str(error))
        self.assertNotIn('Must Not Load',self.worlds(self.catalog()))


if __name__=='__main__':unittest.main(verbosity=2)
