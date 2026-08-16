$test_dir = "tests"
$tests = @(
    @{ name = "test1";       expect = 10 },
    @{ name = "test2";       expect = 7 },
    @{ name = "test3";       expect = 5 },
    @{ name = "test4";       expect = 3 },
    @{ name = "test5";       expect = 3 },
    @{ name = "test6";       expect = -3 },
    @{ name = "test7";       expect = 1 },
    @{ name = "test_scope1"; expect = 1 },
    @{ name = "test_scope2"; expect = 3 },
    @{ name = "test_scope3"; expect = 1 },
    @{ name = "test_scope4"; expect = 3 },
    @{ name = "test_cmp1";   expect = 1 },
    @{ name = "test_cmp2";   expect = 0 },
    @{ name = "test_cmp3";   expect = 1 },
    @{ name = "test_cmp4";   expect = 1 },
    @{ name = "test_cmp5";   expect = 1 },
    @{ name = "test_cmp6";   expect = 0 },
    @{ name = "test_cmp7";   expect = 1 },
    @{ name = "test_cmp8";   expect = 1 },
    @{ name = "test_cmp9";   expect = 1 },
    @{ name = "test_cmp10";  expect = 1 },
    @{ name = "test_if1";    expect = 0 },
    @{ name = "test_if2";    expect = 1 },
    @{ name = "test_if3";    expect = 1 },
    @{ name = "test_if4";    expect = 0 },
    @{ name = "test_if5";    expect = 2 },
    @{ name = "test_if6";    expect = 100 },
    @{ name = "test_while1"; expect = 45 },
    @{ name = "test_while2"; expect = 5 },
    @{ name = "test_while3"; expect = -2 },
    @{ name = "test_while4"; expect = 25 },
    @{ name = "test_while5"; expect = 5 },
    @{ name = "test_while6"; expect = 5 },
    @{ name = "test_func1";  expect = 2 },
    @{ name = "test_func2";  expect = 6 },
    @{ name = "test_func3";  expect = 5 },
    @{ name = "test_func4";  expect = 10 },
    @{ name = "test_func5";  expect = 6 },
    @{ name = "test_func6";  expect = 55 },
    @{ name = "test_func7";  expect = 7 },
    @{ name = "test_func8";  expect = 6 }
)
$all_pass = $true

foreach ($t in $tests) {
    $name = $t.name
    $exp = $t.expect
    $src = "$test_dir\$name.cmm"
    # intermediates go to temp\ (run from repo root: powershell tests/run_tests.ps1)
    $asm = "temp\$name.s"
    $bin = "temp\$name.bin"
    Write-Host ""
    Write-Host "===== $name (expect $exp) =====" -ForegroundColor Cyan

    .\compiler.exe $src $asm 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL] compile error" -ForegroundColor Red
        $all_pass = $false
        continue
    }

    .\assembler.exe $asm -o $bin 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL] assemble error" -ForegroundColor Red
        $all_pass = $false
        continue
    }

    $output = .\simulator.exe $bin --trace 2>&1 | Select-Object -Last 1
    Write-Host $output

    if ($output -match "a0=(-?\d+)") {
        $a0 = [int]$Matches[1]
        if ($a0 -eq $exp) {
            Write-Host "[PASS] a0=$a0" -ForegroundColor Green
        } else {
            Write-Host "[FAIL] a0=$a0, expected $exp" -ForegroundColor Red
            $all_pass = $false
        }
    } else {
        Write-Host "[FAIL] no a0 in output" -ForegroundColor Red
        $all_pass = $false
    }
}

Write-Host ""
if ($all_pass) {
    Write-Host "===== ALL TESTS PASSED =====" -ForegroundColor Green
} else {
    Write-Host "===== SOME TESTS FAILED =====" -ForegroundColor Red
}
