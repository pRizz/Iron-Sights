# Save backup dependencies

Unmodified headers used only by the native save-transfer adapter:

- [picojson](https://github.com/kazuho/picojson), v1.3.0, commit `25fc213cca61ea22b3c2e4db8def9927562ba5f7`, `picojson/picojson.h`. BSD-2-Clause license retained in the header. SHA-256: `5ddf7276d04926da7be243e7af49258e78cc27278ee9097ba45b942c7a6b5f9d`.
- [PicoSHA2](https://github.com/okdshin/PicoSHA2), commit `161cb3fc4170fa7a3eca9e582cebd27cc4d1fe29`, `picosha2/picosha2.h`. MIT license retained in the header. SHA-256: `b13c180161ffac8d0adc81e033e493c409457c4d1258ab9781ac80579ba3bdd8`.

No runtime dependency or network access is required. Updates must preserve the portable backup format and pass the cross-target backup regression.
