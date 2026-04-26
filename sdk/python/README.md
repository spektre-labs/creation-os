# creation-os (Python)

Thin HTTP client for the local `cos serve` API.

```bash
pip install "git+https://github.com/spektre-labs/creation-os.git#subdirectory=sdk/python"
# or from a checkout:
pip install ./sdk/python
```

```python
from creation_os import CreationOS

cos = CreationOS()
print(cos.gate("What is 2+2?"))
```

Requires a running `./cos serve --port 3001` (or `cos-serve`) on the same machine by default.
