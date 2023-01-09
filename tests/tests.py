import socket, requests, unittest, random, os
from multiprocessing import Process
from hashlib import sha256

EXECUTABLE_PATH = os.path.join(os.getenv("HOME"), "Src/enzyme/enzyme")

def fork_proc(env={}):
    srv_port = random.randint(1000, 65535)
    child_pid = os.fork()

    if child_pid == 0:
        for k in env.keys():
            os.environ[k] = env[k]

        os.chdir("./srv")
        os.execl(EXECUTABLE_PATH, EXECUTABLE_PATH, "localhost", str(srv_port))
    
    return srv_port, child_pid

def downloader_task(port, path, output):
    r = requests.get(f"http://localhost:{port}{path}")

    assert r.ok
    assert r.reason == "OK"

    with open(output, "wb") as o:
        o.write(r.content)

def hash_file(path):
    r = b""
    algo = sha256()

    with open(path, "rb") as f:
        while True:
            r = f.read(16384)
            if len(r) == 0:
                return algo.digest()

            algo.update(r)

class TestBasicHTTP(unittest.TestCase):
    def setUp(self):
        self.srv_port, self.child_pid = fork_proc()

    def tearDown(self):
        os.kill(self.child_pid, 15)
        os.wait()

    def test_index(self):
        r1 = requests.get(f"http://localhost:{self.srv_port}/index.html")
        self.assertTrue(r1.ok)

        r2 = requests.get(f"http://localhost:{self.srv_port}/")
        self.assertTrue(r2.ok)

        self.assertEqual(r1.reason, "OK")

        with open("./srv/index.html", "rb") as f:
            index_content = f.read()

        self.assertEqual(r1.content, index_content)
        self.assertEqual(r1.content, r2.content)

    def test_404(self):
        r = requests.get(f"http://localhost:{self.srv_port}/yiff_owo.wasm")
        self.assertFalse(r.ok)
        self.assertEqual(r.reason, "Not Found")
        self.assertEqual(r.content, b"404 Not Found\n")

class TestMultiClients(unittest.TestCase):
    def setUp(self):
        self.srv_port, self.child_pid = fork_proc({"DEBUG_SLEEPS": ""})

        try:
            os.remove("/tmp/dl1.bin")
            os.remove("/tmp/dl2.bin")
        except FileNotFoundError:
            None

    def tearDown(self):
        os.kill(self.child_pid, 15)
        os.wait()

        try:
            os.remove("/tmp/dl1.bin")
            os.remove("/tmp/dl2.bin")
        except FileNotFoundError:
            None
    
    def test_multidown(self):
        threads = []

        threads.append(Process(target=downloader_task, args=(self.srv_port, "/images/test_img.jpg", "/tmp/dl1.bin")))
        threads.append(Process(target=downloader_task, args=(self.srv_port, "/images/test_img.jpg", "/tmp/dl2.bin")))

        for t in threads:
            t.start()

        for t in threads:
            t.join()

        h1 = hash_file("/tmp/dl1.bin")
        h2 = hash_file("/tmp/dl2.bin")
        self.assertEqual(h1, h2)

        h3 = hash_file("./srv/images/test_img.jpg")
        self.assertEqual(h1, h3)

if __name__ == "__main__":
    unittest.main()
