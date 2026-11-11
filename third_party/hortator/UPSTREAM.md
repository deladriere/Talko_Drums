# Upstream

Copied on 11 November 2026, unmodified, from
https://github.com/DeadActive/hortator at commit
`44c9ad4aa57bebf63f412bd552e57143c30b4065`.

| File here | Path in Hortator |
|---|---|
| `dm_*.c`, `dm_state.h`, `dsp.c`, `phys_dsp.c`, `preset_bank.h` | `firmware/src/` (same names) |
| `gen_tables.py` | `tools/gen_tables.py` |
| `LICENSE`, `LICENSING.md` | top level |
| `LICENSES/MIT-DaisySP.txt`, `LICENSES/MIT-Mutable-Instruments.txt` | `LICENSES/` |

`felucca_tables.h` was generated here with `python3 gen_tables.py felucca_tables.h`;
regenerating it gives the same bytes.

SHA-256:

```text
3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986  LICENSE
7cd553a6dfade7ed443627685a8754578aae7d25f34579c7211bba82998f0258  LICENSING.md
0de5c16e3fbd2d942794e7e830715b651a2d25859dc548cb648dd0df762f4a09  dm_b2trx.c
b5ddfcb5a4076ada08136fc0c46ef9670d336214dbcb58b913a937f4517ba497  dm_bdefm.c
f685a87f167eeb8f76cd248d8e677edfba60552221ac6559b7d4633e3a907dc7  dm_cltrx.c
a044428d99d00e164e4db3986740ff4851b8c0bfa93f3e6bda8cdd9e810ba1be  dm_dsp.c
32572b6f86a04e95e6369c271f8c304a5229eaa7ad3299f2d951c37b8dde656c  dm_efmm.c
b89b89ac973649ff4055abfaad63cc113273eda2a578d063a8f4a16cd2d62a0c  dm_efmn.c
7d683bbef7c7986a7030fe68dbd05011664b3ff3de6d5d69f5d187b92506ed66  dm_fm.c
278805430986d4cd641fd4c454fc2bc6ac8d0403d65486e17e07be4d97608fa1  dm_kick.c
828f997512d46d8a6581aeaca7810b112ad5749bc92b524ed95b951c4a78ada6  dm_ktrx.c
ed30c0d09d15d97d0fd58de1337f549adcef5364f2290da24dab912b0d7610b6  dm_memb.c
f91cc206b99488e742d9474b8cbb510fbbdc3dbab1cc6dfc06267dd7b051ac67  dm_metal.c
e2caf670f795f5c9d1dbb5b50c9034197a4f3e235091dc0357459165156df5f4  dm_perc.c
62cbe3528cae1fb8c8a8821e39a37abcbfc0348a40f34df71117a71fc0efbbd7  dm_rstrx.c
cd2e24a04be756ffb2f30ecec6727f3443b1066b647582f02961510090dde66c  dm_sdtrx.c
8936833b6d139acdb8eef64b30ddd7cef40a14d07cc5a6edf686a93f6636ce6e  dm_snare.c
67e27197cbcff5bfd2202e1921362a36e2cf18dd5b85c1565f2075ccd5848404  dm_state.h
e6c781070f736ed8ac7f55799428f85c499fcf45af60e911246b8d0a7f0feca6  dm_trxcb.c
8b3aed1ccaf785a6907176fdca4828a740bae0a51e61b1d8489cf2ad8ba6a328  dm_trxm.c
e738525ca2295eead4ce9d411665fbdbdd94562446303b431e5789542ca69da2  dm_trxn.c
066d781187bd1d2d7dc4f3bb4d68e3b2a1fa80f8d681e74b143af529636b8c4f  dm_xttrx.c
b70f0ef637b74ea9d08e17806ed464bfb1e40573308bcbbb75a54483947674d0  dsp.c
e43e8258b79c85bb312e7e0cf6002974f04f52ff874d75a16f1326231ae614c1  felucca_tables.h
055bc5945479587fcae6f8c41cea7f35b984150b9ac66faea935ff67b2ffa85c  gen_tables.py
16ffa8d84383d7ee51ee8acc5b306bcd1bf59a1247b2e23c6c32d183e8726ec7  phys_dsp.c
c1c6ecc849f788fcbbd75b6cef21e60a305138b04faa8a2fd47afd289270ff0c  preset_bank.h
30a3ae5c90906e6243283e90ecdccdc12e266663d819e7068c27eaceb839030c  LICENSES/MIT-DaisySP.txt
974c1486fe7d31368bc9aa1c805deae0737dd5881155cd0e7a5ff18b2a359558  LICENSES/MIT-Mutable-Instruments.txt
```
