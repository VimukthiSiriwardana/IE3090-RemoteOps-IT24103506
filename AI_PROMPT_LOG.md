# IE3090 Network Programming
# RemoteOps – AI Prompt Log

**Registration Number:** IT24103506
**Name:** Siriwardana S.A.D.V.I
**Project:** RemoteOps
**Module:** IE3090 – Network Programming

---

## 1. Purpose

This prompt log records the substantive AI interactions used during the development of the RemoteOps project.

I mainly used ChatGPT as a technical reference and troubleshooting resource when I encountered errors, unexpected behaviour, or technical concepts that required further clarification. The use of AI was mainly related to implementation and testing, particularly when investigating problems or checking my understanding of networking concepts.

The RemoteOps application was implemented and tested in my Linux environment. When I received a suggestion from ChatGPT, I checked it against the assignment requirements, my existing source code, and the actual behaviour of the program before deciding whether to apply or adapt it.

The entries below summarise the substantive AI interactions used during development.

---

## 2. AI Interaction Log

### Interaction 1 – TCP Stream and `recv()` Behaviour

**AI Tool:** ChatGPT

**Prompt / Request (summary):**
I asked ChatGPT to explain why a TCP server may call `recv()` multiple times for one logical message and why the received data may not always match the way the sender transmitted it.

**AI Guidance Received:**
ChatGPT explained that TCP is a stream-oriented protocol and does not preserve application-level message boundaries. A logical message may therefore be received in fragments, while multiple messages may also be received together.

**How I Used or Changed the Output:**
I used the explanation to better understand the command receiving logic in my RemoteOps Agent and the requirement to handle fragmented and coalesced TCP commands.

**Verification:**
I tested the RemoteOps application using fragmented and coalesced command cases. The Agent correctly processed the commands after the receiving logic was checked and tested.

---

### Interaction 2 – TCP Buffer and String Handling

**AI Tool:** ChatGPT

**Prompt / Request (summary):**
I asked ChatGPT about a problem I encountered while working with `recv()` and a small receive buffer, including why received TCP data may require multiple reads and how the null terminator should be handled.

**AI Guidance Received:**
ChatGPT explained the difference between received byte data and C strings and why the application should not assume that one `recv()` call contains a complete application message.

**How I Used or Changed the Output:**
I used the explanation as a reference while checking my TCP receiving logic and buffer handling.

**Verification:**
I compiled the program and tested the Controller and Agent again to confirm that commands were being received and processed correctly.

---

### Interaction 3 – PUT/GET File Transfer

**AI Tool:** ChatGPT

**Prompt / Request (summary):**
I asked ChatGPT for help understanding an issue or uncertainty related to receiving and sending the exact number of bytes required during PUT and GET file transfers.

**AI Guidance Received:**
ChatGPT explained that TCP is a byte stream and that file transfer code should continue receiving or sending data until the required number of bytes has been processed rather than assuming one call transfers the complete file.

**How I Used or Changed the Output:**
I used the explanation to check my existing file-transfer implementation and make the required adjustments where necessary.

**Verification:**
I tested PUT and GET using the Controller and verified that files were transferred successfully. I also tested invalid file-transfer cases such as a missing file and an invalid file size.

---

### Interaction 4 – UDP Monitoring Troubleshooting

**AI Tool:** ChatGPT

**Prompt / Request (summary):**
I asked ChatGPT for help when I encountered unexpected behaviour while testing the MONITOR START and MONITOR STOP functionality.

**AI Guidance Received:**
ChatGPT suggested checking the monitoring thread, its stop condition, and how the monitoring process waits between UDP transmissions.

**How I Used or Changed the Output:**
I inspected my existing implementation and used the troubleshooting suggestions to identify the required changes to the monitoring logic. I then made and tested the changes in my own implementation.

**Verification:**
I restarted the Agent and tested MONITOR START. Periodic UDP SYSINFO messages were received successfully. I then tested MONITOR STOP and confirmed that the periodic messages stopped.

---

### Interaction 5 – pthread Concurrency Troubleshooting

**AI Tool:** ChatGPT

**Prompt / Request (summary):**
I asked ChatGPT for help when I encountered a problem or unexpected behaviour while testing simultaneous Controller connections using pthreads.

**AI Guidance Received:**
ChatGPT explained possible causes related to thread handling and suggested areas of the implementation that could be checked.

**How I Used or Changed the Output:**
I inspected my existing pthread implementation and used the explanation as troubleshooting guidance. I made the necessary changes based on my own implementation and testing.

**Verification:**
I compiled the Agent again and tested five Controller connections simultaneously. All five Controllers were able to connect and authenticate successfully.

---

### Interaction 6 – Compiler Warnings and Errors

**AI Tool:** ChatGPT

**Prompt / Request (summary):**
During development, I asked ChatGPT to explain compiler warnings or errors that appeared when compiling the Agent or Controller.

**AI Guidance Received:**
ChatGPT explained possible causes of the warnings or errors and suggested areas of the code that could be inspected.

**How I Used or Changed the Output:**
I checked the suggested causes against my own source code and decided whether a change was actually required. I did not automatically apply every suggestion.

**Verification:**
After making relevant changes, I compiled the program again using GCC and checked the resulting compiler output.

---

### Interaction 7 – Runtime Troubleshooting

**AI Tool:** ChatGPT

**Prompt / Request (summary):**
I used ChatGPT when a feature produced unexpected output during testing and asked for possible reasons for the behaviour.

**AI Guidance Received:**
ChatGPT provided possible explanations and suggested checks that could help identify the cause.

**How I Used or Changed the Output:**
I used these suggestions as troubleshooting guidance, inspected my own code, and made the appropriate correction based on my understanding of the problem.

**Verification:**
I repeated the affected test after making the change and checked the actual Controller output, Agent output, or log file.

---

### Interaction 8 – Logging Troubleshooting

**AI Tool:** ChatGPT

**Prompt / Request (summary):**
I asked ChatGPT for help when checking the Agent activity logging and timestamp behaviour.

**AI Guidance Received:**
ChatGPT helped explain how the timestamp formatting and logging code should work and what to check when the expected log output was not appearing correctly.

**How I Used or Changed the Output:**
I checked my existing `write_log()` implementation and restored or adjusted the relevant logging logic based on the troubleshooting.

**Verification:**
I restarted the Agent and performed several commands. I then inspected `remoteops_IT24103506.log` and verified that timestamped entries were being recorded.

---

### Interaction 9 – Testing and Error Handling

**AI Tool:** ChatGPT

**Prompt / Request (summary):**
I used ChatGPT as a reference when checking whether particular error cases in the RemoteOps application were being handled correctly.

**AI Guidance Received:**
The guidance helped me think about cases such as unauthenticated commands, unauthorised EXEC commands, missing files, and invalid file transfers.

**How I Used or Changed the Output:**
I compared these cases with the assignment requirements and tested them using my own Controller and Agent.

**Verification:**
The following types of error cases were tested:

- Command before authentication
- EXEC command outside the whitelist
- GET for a non-existent file
- Invalid PUT file size

The Agent returned the expected error responses.

---

### Interaction 10 – Implementation Report and Documentation Review

**AI Tool:** ChatGPT

**Prompt / Request (summary):**
I asked ChatGPT to review parts of my Implementation Report and project documentation against the assignment requirements and identify areas that needed clarification, correction, or better organisation.

**AI Guidance Received:**
ChatGPT helped identify areas that could be improved, including report structure, figure numbering, testing evidence, personalisation details, and consistency with the assignment requirements.

**How I Used or Changed the Output:**
I reviewed the suggestions and made the appropriate changes to the report and documentation. I retained only changes that matched the assignment requirements and my actual implementation.

**Verification:**
I checked the final report and documentation against the assignment brief and my implemented Agent and Controller functionality before preparing the submission.

---

## 3. How AI Was Used

My use of AI was focused mainly on technical reference, troubleshooting, clarification, and selected documentation review during implementation and testing.

I used ChatGPT when I encountered errors, unexpected program behaviour, or networking concepts that I wanted to understand more clearly. I also used it occasionally to discuss possible ways of investigating a problem and to review parts of the project documentation against the assignment requirements.

When an AI suggestion was relevant, I checked it against the assignment specification and my existing implementation. I then adapted the suggestion where necessary and tested the result in my Linux environment before considering the problem resolved.

---

## 4. Verification of AI-Assisted Changes

For programming-related troubleshooting, I followed a simple process:

1. Identify the error or unexpected behaviour.
2. Ask ChatGPT for an explanation or possible cause.
3. Check the suggestion against my own source code.
4. Make the required change myself.
5. Compile the Agent or Controller again.
6. Repeat the relevant test.
7. Check the actual output or log file.

This helped me use AI as a support tool while still understanding and verifying the implementation.

---

## 5. Final Statement

ChatGPT was used during the RemoteOps development mainly for technical reference, troubleshooting, clarification, and selected documentation review. I reviewed and tested AI-assisted suggestions before using them and remained responsible for the final implementation decisions, source code, configuration, testing, and submitted project.
