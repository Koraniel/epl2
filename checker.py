import sys


expected = '''test_simple
Done
test_multiple
Done
Done
Done
test_recursive
Done
Done
Done
test_yield_one
Done
test_yield_many
Done
Done
Done
test_simple_server_client
Hello, hell!
Hello, hell!
test_server_many_clients
Done
test_server_many_clients2
Done
Done
Done
Done
Done
test_exceptionaltest
Done
Done
Done
Done
Done
test_supertest
Done
'''

res = 0
with open(sys.argv[1]) as f:
    expect_iter = iter(expected.strip().split('\n'))
    for line in f:
        expect = next(expect_iter).strip()
        line = line.strip()
        if line != expect:
            print(line, '!=', expect)
            res -= 10
            break
        if expect.startswith('test_'):
            res += 10
    try:
        expect = next(expect_iter)
        if not expect.startswith('test_'):
            res -= 10
    except:
        pass

if res < 0:
    res = 0

print(res)

with open('res.txt', 'w') as f:
    f.write(str(res))
