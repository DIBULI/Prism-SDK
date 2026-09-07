# Third-party notices

The macOS arm64 runtime includes libusb 1.0.30 as
`runtime/macos-arm64/libusb-1.0.0.dylib`.

- Project: <https://libusb.info/>
- Source release: <https://github.com/libusb/libusb/releases/tag/v1.0.30>
- Source archive SHA-256:
  `fea36f34f9156400209595e300840767ab1a385ede1dc7ee893015aea9c6dbaf`
- License text: `runtime/macos-arm64/libusb-COPYING.txt`

The Linux x86-64 and ARM64 shared runtimes statically include OpenSSL 1.1.1f from Ubuntu
package revision `1.1.1f-1ubuntu2.24`.

- Project: <https://www.openssl.org/>
- Ubuntu source package:
  <https://launchpad.net/ubuntu/+source/openssl/1.1.1f-1ubuntu2.24>
- License text: `runtime/linux-x64/openssl-LICENSE.txt`
- ARM64 license text: `runtime/linux-arm64/openssl-LICENSE.txt`

This product includes software developed by the OpenSSL Project for use in the
OpenSSL Toolkit. This product includes cryptographic software written by Eric
Young (eay@cryptsoft.com).

The RK-local ARM64 static archive also embeds Ubuntu OpenSSL 1.1.1f
(package revision 1.1.1f-1ubuntu2.24) and miniz for joint ZIP validation.
See runtime/linux-arm64/openssl-LICENSE.txt and runtime/linux-arm64/miniz-LICENSE.txt.
No dynamic libcrypto/libssl is required by the RK-local archive.
