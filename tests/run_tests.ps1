$test_dir = "tests"
$tests = @(
    # arithmetic
    @{ name = "arithmetic/test";      expect = 10 },
    @{ name = "arithmetic/test1";     expect = 10 },
    @{ name = "arithmetic/test2";     expect = 7 },
    @{ name = "arithmetic/test3";     expect = 5 },
    @{ name = "arithmetic/test4";     expect = 3 },
    @{ name = "arithmetic/test5";     expect = 3 },
    @{ name = "arithmetic/test6";     expect = -3 },
    @{ name = "arithmetic/test7";     expect = 1 },
    # scope
    @{ name = "scope/test_scope1";    expect = 1 },
    @{ name = "scope/test_scope2";    expect = 3 },
    @{ name = "scope/test_scope3";    expect = 1 },
    @{ name = "scope/test_scope4";    expect = 3 },
    # comparison
    @{ name = "comparison/test_cmp1";  expect = 1 },
    @{ name = "comparison/test_cmp2";  expect = 0 },
    @{ name = "comparison/test_cmp3";  expect = 1 },
    @{ name = "comparison/test_cmp4";  expect = 1 },
    @{ name = "comparison/test_cmp5";  expect = 1 },
    @{ name = "comparison/test_cmp6";  expect = 0 },
    @{ name = "comparison/test_cmp7";  expect = 1 },
    @{ name = "comparison/test_cmp8";  expect = 1 },
    @{ name = "comparison/test_cmp9";  expect = 1 },
    @{ name = "comparison/test_cmp10"; expect = 1 },
    # if
    @{ name = "if/test_if1";    expect = 0 },
    @{ name = "if/test_if2";    expect = 1 },
    @{ name = "if/test_if3";    expect = 1 },
    @{ name = "if/test_if4";    expect = 0 },
    @{ name = "if/test_if5";    expect = 2 },
    @{ name = "if/test_if6";    expect = 100 },
    # while
    @{ name = "while/test_while1"; expect = 45 },
    @{ name = "while/test_while2"; expect = 5 },
    @{ name = "while/test_while3"; expect = -2 },
    @{ name = "while/test_while4"; expect = 25 },
    @{ name = "while/test_while5"; expect = 5 },
    @{ name = "while/test_while6"; expect = 5 },
    # function
    @{ name = "function/test_func1";  expect = 2 },
    @{ name = "function/test_func2";  expect = 6 },
    @{ name = "function/test_func3";  expect = 5 },
    @{ name = "function/test_func4";  expect = 10 },
    @{ name = "function/test_func5";  expect = 6 },
    @{ name = "function/test_func6";  expect = 55 },
    @{ name = "function/test_func7";  expect = 7 },
    @{ name = "function/test_func8";  expect = 6 },
    # float
    @{ name = "float/literal"; expect_fa0 = 2.5 },
    @{ name = "float/add";    expect_fa0 = 2.5 },
    @{ name = "float/sub";    expect_fa0 = 1.5 },
    @{ name = "float/mul";    expect_fa0 = 3.0 },
    @{ name = "float/div";    expect_fa0 = 1.5 },
    @{ name = "float/neg";    expect_fa0 = -1.5 },
    @{ name = "float/lt";     expect = 1 },
    @{ name = "float/eq";     expect = 1 },
    @{ name = "float/func";   expect_fa0 = 4.0 },
    @{ name = "float/param";  expect_fa0 = 4.0 },
    # global
    @{ name = "global/read";       expect = 10 },
    @{ name = "global/write";      expect = 7 },
    @{ name = "global/init";       expect = 42 },
    @{ name = "global/cross_func"; expect = 10 },
    @{ name = "global/shadow";     expect = 2 },
    @{ name = "global/expr";       expect = 7 },
    @{ name = "global/float_global"; expect_fa0 = 2.5 },
    @{ name = "global/float_write";  expect_fa0 = 1.5 }
)
$all_pass = $true

foreach ($t in $tests) {
    $name = $t.name
    $has_a0 = $t.ContainsKey("expect")
    $has_fa0 = $t.ContainsKey("expect_fa0")
    $expect_str = if ($has_a0) { "a0=$($t.expect)" } elseif ($has_fa0) { "fa0=$($t.expect_fa0)" } else { "?" }
    $base = $name -replace '[/\\]','_'
    $src = "$test_dir\$name.cmm"
    $asm = "temp\$base.s"
    $bin = "temp\$base.bin"
    Write-Host ""
    Write-Host "===== $name (expect $expect_str) =====" -ForegroundColor Cyan

    $compile_output = .\compiler.exe $src $asm 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL] compile error" -ForegroundColor Red
        Write-Host ($compile_output -join "`n") -ForegroundColor DarkGray
        $all_pass = $false
        continue
    }

    $assemble_output = .\assembler.exe $asm -o $bin 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL] assemble error" -ForegroundColor Red
        Write-Host ($assemble_output -join "`n") -ForegroundColor DarkGray
        $all_pass = $false
        continue
    }

    $output = .\simulator.exe $bin --trace 2>&1 | Select-Object -Last 1
    Write-Host $output

    $matched = $false
    if ($has_a0 -and $output -match "a0=(-?\d+)") {
        $a0 = [int]$Matches[1]
        $matched = $true
        if ($a0 -eq $t.expect) {
            Write-Host "[PASS] a0=$a0" -ForegroundColor Green
        } else {
            Write-Host "[FAIL] a0=$a0, expected $($t.expect)" -ForegroundColor Red
            $all_pass = $false
        }
    }
    if ($has_fa0 -and $output -match "fa0=([-\d\.]+|nan|inf|-inf)") {
        $fa0 = [double]$Matches[1]
        $matched = $true
        if ([math]::Abs($fa0 - $t.expect_fa0) -lt 1e-6) {
            Write-Host "[PASS] fa0=$fa0" -ForegroundColor Green
        } else {
            Write-Host "[FAIL] fa0=$fa0, expected $($t.expect_fa0)" -ForegroundColor Red
            $all_pass = $false
        }
    }
    if (-not $matched) {
        Write-Host "[FAIL] no expected register in output" -ForegroundColor Red
        $all_pass = $false
    }
}

Write-Host ""
if ($all_pass) {
    Write-Host "===== ALL TESTS PASSED =====" -ForegroundColor Green
} else {
    Write-Host "===== SOME TESTS FAILED =====" -ForegroundColor Red
}
