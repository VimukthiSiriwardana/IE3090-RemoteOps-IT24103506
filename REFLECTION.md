# IE3090 Network Programming
# RemoteOps – Structured Reflection

**Registration Number:** IT24103506
**Name:** Siriwardana S.A.D.V.I
**Project:** RemoteOps

---

## Reflection

During the development of the RemoteOps project, I used ChatGPT primarily as a technical reference and troubleshooting resource. I used it at different stages of implementation and testing when I encountered errors, unexpected behaviour, or networking concepts that required further clarification. For example, I referred to it while investigating TCP stream behaviour, file-transfer handling, UDP monitoring, pthread-based concurrency, and compiler or runtime issues.

The tool was particularly useful for explaining possible causes of technical problems and suggesting approaches for investigating them. For example, when working with TCP, I needed to understand why `recv()` could return partial data or why multiple commands could be received together. The explanation helped me understand TCP as a continuous byte stream and the importance of appropriate message framing. Similar troubleshooting discussions were useful when checking file transfers, UDP monitoring, concurrent Controller connections, and logging behaviour.

However, the suggestions were not always directly applicable to my implementation. Some responses were based on assumptions that did not fully match the protocol or structure of my application. I therefore evaluated the suggestions against the assignment requirements and my existing implementation before deciding whether to apply them. Where a suggested approach was relevant, I adapted it to the structure of my program rather than applying it without verification.

I also used practical testing to evaluate the usefulness of the suggestions. After making a change, I compiled the program and tested the affected functionality in the Linux environment. When an approach did not produce the expected behaviour, I investigated the implementation further and either modified the approach or used an alternative solution. This process helped me understand the difference between a general technical suggestion and a solution that was appropriate for my specific implementation.

The assignment also improved my understanding of network programming through practical implementation. I gained a clearer understanding of TCP stream framing, exact byte handling during file transfers, concurrent client connections using pthreads, authentication, error handling, and connection termination. Implementing UDP monitoring also helped me understand the practical difference between connection-oriented TCP communication and connectionless UDP communication.

Overall, the project improved my confidence in C socket programming and troubleshooting. It also showed me the importance of combining theoretical knowledge with practical testing. Rather than relying only on expected behaviour, I learned to examine actual program output, identify the cause of problems, make appropriate changes, and verify the results through repeated testing.
