# hfpe

high frequency pe registration

## Build

```bash
cmake --preset release
cmake --build build-release
```

## Usage

```bash
export KERB="your_kerberos"
export KERB_PASSWORD="your_password"
export MIT_ID="9XXXXXXXX"
export PE_SECTION_NAME="PE.0613-1"

./build-release/bot
```

### Environment Variables

| Variable               | Required | Default                   | Description                         |
| ---------------------- | -------- | ------------------------- | ----------------------------------- |
| `KERB`                 | Yes      | -                         | Kerberos username                   |
| `KERB_PASSWORD`        | Yes      | -                         | Kerberos password                   |
| `MIT_ID`               | Yes      | -                         | MIT ID number                       |
| `PE_SECTION_NAME`      | Yes      | -                         | P.E. section name, e.g. `PE.0XXX-X` |
| `PE_BASE_URL`          | No       | `https://eduapps.mit.edu` | Base URL                            |
| `PE_REGISTRATION_TIME` | No       | `08:00`                   | Registration time (HH:MM)           |

## Testing

```bash
python mock_server.py --failure-rate 0.9 --slow-rate 0.4
```

```bash
PE_BASE_URL=http://localhost:8080 PE_REGISTRATION_TIME=XX:XX ./build-release/bot
```
