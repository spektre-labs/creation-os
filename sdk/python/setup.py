from setuptools import find_packages, setup

setup(
    name="creation-os",
    version="0.1.0",
    packages=find_packages(exclude=("tests",)),
    install_requires=["requests"],
    description="Python SDK for Creation OS σ-gate (cos-serve HTTP client)",
    url="https://github.com/spektre-labs/creation-os",
    license="LicenseRef-SCSL-1.0 OR AGPL-3.0-only",
    python_requires=">=3.9",
)
