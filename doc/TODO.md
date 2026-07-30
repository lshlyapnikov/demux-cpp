# TODO

- [ ] Extract reader and writer loops.
- [ ] Refactor the shm_demux example so you can run it without calculating message hashing,
      hashing shows up as the CPU hot spot. So you need to measure latency without the hashing.
      However, it is useful as the integration test that you run as part of CI pipeline,
      remember you check sent and received message hashes.
