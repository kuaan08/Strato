# Strato Adreno

Fork de desenvolvimento do [Strato](https://github.com/strato-emu/strato), projeto que continua o trabalho do Skyline. Esta variante está sendo preparada para GPUs Adreno em Android, com o Poco X5 Pro 5G como primeiro aparelho-alvo.

O código já contém suporte genérico a Vulkan, carregamento opcional de drivers compatíveis e o caminho de memória KGSL para dispositivos Adreno. O APK desta branch não inclui um driver Vulkan customizado: por padrão, usa o driver Vulkan fornecido pelo Android/aparelho. A detecção por capacidades e identificadores Vulkan deve continuar sendo a base para ajustes de compatibilidade, em vez de presumir o comportamento apenas pelo nome do telefone.

## Compilação

O GitHub Actions compila a variante `mainlineRelease` ao receber push em `main` ou em branches `release/**`, em pull requests para `main` e também sob demanda. O workflow baixa os submódulos, configura Java 17, NDK `26.1.10909125` e CMake 3.22.1, executa os testes unitários Android e publica o APK como artefato da execução. Para baixar, abra a execução em **Actions** e use a seção **Artifacts**.

Para compilar localmente, clone incluindo os submódulos e execute `./gradlew assembleMainlineRelease`. Consulte [`BUILDING.md`](BUILDING.md) para configurar o Android Studio. O APK gerado pelo Actions é assinado com a chave de depuração do Gradle e serve para testes; não é uma build de distribuição assinada para publicação.

O pacote Android foi separado como `org.stratoemu.stratoadreno`. Isso permite instalar a variante em paralelo a outros pacotes Strato, mas os dados e saves internos não são migrados automaticamente entre IDs de pacote.

## Perfil Poco X5 Pro 5G

O aparelho é o alvo inicial de validação, mas esta branch ainda não deve ser descrita como otimizada ou validada nele. Ajustes de clocks, importação direta de memória, resolução interna, sincronização e opções específicas de Vulkan podem elevar temperatura, consumo, travamentos ou defeitos gráficos. Por isso, devem ser avaliados com medições repetíveis no próprio telefone antes de virarem padrões.

Ao relatar um teste, registre o commit/APK, versão do Android e do driver Vulkan, jogo e cena usados, configuração gráfica, FPS/frametime, temperatura aproximada e se houve artefatos ou travamentos. Compare primeiro com o driver do sistema e as configurações padrão.

## Créditos e licença

O projeto deriva do Strato e preserva os créditos dos autores e colaboradores de Strato/Skyline. Os avisos de direitos autorais e as licenças de bibliotecas e submódulos permanecem junto aos respectivos componentes. O código Strato está sob GNU GPL v3 ou posterior; consulte [`LICENSE.md`](LICENSE.md) e as licenças de terceiros antes de redistribuir.
