# Contribution 
Contributions to this project are welcomed and encouraged. If you want to
contribute, make a pull request in this repository. Be warned, however, there 
are some fairly strict requirements to be met for any contribution.

## Maintainer Approval
At a very basic level, the maintainer of this codebase must agree with changes 
being made in contributor pull requests. Maintainers reserve the right to reject 
any pull request for any reason.

## Coding Standards
[All contributions are required to match these coding standards](coding_standards.md)

## Testing 
Contributions must add/maintain unit tests as necessary. 
 
If the changes add new API, each new function call must be tested. As a general 
rule, favor having at least one new test for each API call. Additionally, unit 
tests for concurrent features must prove they work in
1. Single threaded, non-coroutine environment
2. Single threaded, coroutine environment (running in a `ccc::scheduler`)
3. Multi threaded, non-coroutine environment
3. Multi threaded, coroutine environment (running in multiple `ccc::scheduler`s) 

If the changes implement similar functionality to a comparable feature already 
present in the project, such as adding a new kind of channel, then the unit test 
coverage must at least *match* the coverage of the similar features. 

## Documentation 
Any API or behavioral changes must be documented in the various .md files in 
this project.

Furthermore, for any new API at least one new simple example demonstrating usage 
should be provided, in a similar fashion present in the documentation. This 
means the given example must be included in the `ex/` directory and incorporated 
into the build environment so it runs as part of `continuous_integration.py`.

## Integration 
Any potential contribution must pass the continuous integration test made on 
all commits to pull reequests. Aside from automatic continuous integration 
launched when a change is committed to a pull request, all example code and 
unit tests can be run via:
```
python ./continuous_integration.py
```

## Performance 
This is a very performance sensitive project. Any changes should be checked to 
not cause performance degradation. For now, this is done manually by reviewing 
unit test integration results from master and comparing to a pull request's 
integration results.

Pull requests to implement better performance metrics are welcome.

## Commit 
Before a pull request will be accepted, it must be rebased into a single commit.
A commit message should follow this pattern:
```
[github username] [title summary of changes]

[a clear paragraph or lengthier description of the changes]
```
Commit messages should be free of typos and are subject to changes on the 
maintainer's request.

## Description 
The description must contain the commit message text as well as a Definition of 
Done table:
```
##### Definition of Done
| Requirement | Status |
|-------------| :----: |
| Have provided a correct commit message | :heavy_minus_sign: |  
| Continuous Integration Passed | :heavy_minus_sign: |  
| Provided Unit Test Coverage | :heavy_minus_sign: |  
| Updated Documentation | :heavy_minus_sign: |  
| Changes do not degrade performance. If performance is degraded, have
confirmed with maintainer that changes are acceptable | :heavy_minus_sign: |  
```

Status icons meaning:
```
Available:   :heavy_minus_sign:
In Progress: :heavy_plus_sign:
Blocked:     :heavy_exclamation_mark:
Done:        :heavy_check_mark:
``` 

### An example commit
dennbla Implement the spectacular new foo_channel

foo_channel is a new object which allows users to finally use the well 
known foo feature to communicate. It implements base_channel, and has 
new features faa and fii which have been documented in the md documentation,
a new example program example_NNN, and has its own unit test file 
foo_channel_ut.cpp integrated into the greater unit test build.


##### Definition of Done
| Requirement | Status |
|-------------| :----: |
| Have provided a correct commit message | :heavy_check_mark: |  
| Continuous Integration Passed | :heavy_check_mark: |  
| Provided Unit Test Coverage | :heavy_check_mark: |  
| Updated Documentation | :heavy_check_mark: |  
| Changes do not degrade performance. If performance is degraded, have confirmed with maintainer that changes are acceptable | :heavy_check_mark: |  

## Conclusion 
Once the above is complete for a pull request, the maintainer is responsible for 
a final confirmation/denial and for merging the changes to master.
