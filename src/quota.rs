use std::fmt;
use std::str::FromStr;
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParseSizeError {
    kind: SizeErrorKind,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum SizeErrorKind {
    Empty,
    InvalidQuota,
    NegOverflow,
}

impl ParseSizeError {
    pub fn kind(&self) -> &SizeErrorKind {
        &self.kind
    }
    #[doc(hidden)]
    pub fn __description(&self) -> &str {
        match self.kind {
            SizeErrorKind::Empty => "cannot parse size from empty string",
            SizeErrorKind::InvalidQuota => "invalid size",
            SizeErrorKind::NegOverflow => "size too small",
        }
    }
}

impl fmt::Display for ParseSizeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> Result<(), std::fmt::Error> {
        self.__description().fmt(f)
    }
}

pub trait Size: PartialOrd + PartialOrd {
    fn safe_into_size_bytes(&self) -> Result<usize, ParseSizeError>;
    fn into_size_bytes(&self) -> usize {
        match self.safe_into_size_bytes() {
            Ok(b) => b,
            Err(e) => panic!("{}", e),
        }
    }
}

pub struct AutoSize {
    value: f64,
    unit: Unit,
}

impl AutoSize {
    pub fn new<V>(value: V, unit: Unit) -> Result<AutoSize, ParseSizeError>
    where
        V: Into<f64>,
    {
        let value = value.into();
        if !value.is_normal() && value != 0.0 {
            return Err(ParseSizeError {
                kind: SizeErrorKind::NegOverflow,
            });
        }
        Ok(AutoSize { value, unit })
    }
}

impl Size for AutoSize {
    fn safe_into_size_bytes(&self) -> Result<usize, ParseSizeError> {
        Ok((self.value * (self.unit.into_size_bytes() as f64)) as usize)
    }
}

impl PartialOrd for AutoSize {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        self.into_size_bytes().partial_cmp(&other.into_size_bytes())
    }
}

impl PartialEq for AutoSize {
    fn eq(&self, other: &Self) -> bool {
        self.into_size_bytes() == other.into_size_bytes()
    }
}

impl fmt::Display for AutoSize {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "{}{}", self.value, self.unit)
    }
}

impl FromStr for AutoSize {
    type Err = ParseSizeError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        if s == "" {
            return Err(ParseSizeError {
                kind: SizeErrorKind::Empty,
            });
        }
        let mut pos = s.len();
        for c in s.chars().rev() {
            if c >= '0' && c <= '9' {
                break;
            }
            pos = pos - 1;
        }
        let (value_str, unit_str) = s.split_at(pos);
        AutoSize::new(
            value_str.parse::<f64>().map_err(|_| ParseSizeError {
                kind: SizeErrorKind::InvalidQuota,
            })?,
            unit_str.parse()?,
        )
    }
}

impl Size for String {
    fn safe_into_size_bytes(&self) -> Result<usize, ParseSizeError> {
        self.parse::<AutoSize>()?.safe_into_size_bytes()
    }
}

impl Size for &str {
    fn safe_into_size_bytes(&self) -> Result<usize, ParseSizeError> {
        self.parse::<AutoSize>()?.safe_into_size_bytes()
    }
}

impl Size for &[u8] {
    fn safe_into_size_bytes(&self) -> Result<usize, ParseSizeError> {
        String::from_utf8_lossy(self)
            .parse::<AutoSize>()?
            .safe_into_size_bytes()
    }
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum Unit {
    /// 1 B
    Byte,

    /// 1,000 kB
    Kilobyte,

    /// 1,000,000 (1000^2) MB
    Megabyte,

    /// 1,000,000,000 (1000^3) GB
    Gigabyte,

    /// 1,000,000,000,000 (1000^4) TB
    Terabyte,

    /// 1,024 (1024^1) KiB/KB/K
    Kibibyte,
    /// 1,048,576 (1024^2) MiB/M
    Mebibyte,

    /// 1,073,741,824 (1024^3) GiB/G
    Gigibyte,

    /// 1,099,511,627,776 (1024^4) TiB/T
    Tebibyte,
}

impl Unit {
    fn into_size_bytes(self) -> usize {
        match self {
            Unit::Byte => 1,

            Unit::Kilobyte => 1000,
            Unit::Megabyte => 1000usize.pow(2),
            Unit::Gigabyte => 1000usize.pow(3),
            Unit::Terabyte => 1000usize.pow(4),

            Unit::Kibibyte => 1024,
            Unit::Mebibyte => 1024usize.pow(2),
            Unit::Gigibyte => 1024usize.pow(3),
            Unit::Tebibyte => 1024usize.pow(4),
        }
    }
}

impl FromStr for Unit {
    type Err = ParseSizeError;

    fn from_str(input: &str) -> Result<Unit, Self::Err> {
        match input {
            "B" | "" => Ok(Unit::Byte),

            "kB" => Ok(Unit::Kilobyte),
            "MB" => Ok(Unit::Megabyte),
            "GB" => Ok(Unit::Gigabyte),
            "TB" => Ok(Unit::Terabyte),

            "KiB" | "KB" | "K" => Ok(Unit::Kibibyte),
            "MiB" | "M" => Ok(Unit::Mebibyte),
            "GiB" | "G" => Ok(Unit::Gigibyte),
            "TiB" | "T" => Ok(Unit::Tebibyte),
            _ => Err(ParseSizeError {
                kind: SizeErrorKind::InvalidQuota,
            }),
        }
    }
}

impl fmt::Display for Unit {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let value = match *self {
            Unit::Byte => "B",

            Unit::Kilobyte => "kB",
            Unit::Megabyte => "MB",
            Unit::Gigabyte => "GB",
            Unit::Terabyte => "TB",

            Unit::Kibibyte => "KiB",
            Unit::Mebibyte => "MiB",
            Unit::Gigibyte => "GiB",
            Unit::Tebibyte => "TiB",
        };

        f.pad(value)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_size() -> Result<(), ParseSizeError> {
        assert_eq!("15".parse::<AutoSize>()?.into_size_bytes(), 15);

        assert_eq!("15K".parse::<AutoSize>()?.into_size_bytes(), 15 * 1024);
        assert_eq!("15KiB".parse::<AutoSize>()?.into_size_bytes(), 15 * 1024);
        assert_eq!("15KB".parse::<AutoSize>()?.into_size_bytes(), 15 * 1024);
        assert_eq!(
            "15M".parse::<AutoSize>()?.into_size_bytes(),
            15 * 1024 * 1024
        );
        assert_eq!(
            "15G".parse::<AutoSize>()?.into_size_bytes(),
            15 * 1024 * 1024 * 1024
        );
        assert_eq!(
            "15T".parse::<AutoSize>()?.into_size_bytes(),
            15 * 1024 * 1024 * 1024 * 1024
        );

        assert_eq!("15kB".parse::<AutoSize>()?.into_size_bytes(), 15 * 1000);
        assert_eq!(
            "15MB".parse::<AutoSize>()?.into_size_bytes(),
            15 * 1000 * 1000
        );
        assert_eq!(
            "15GB".parse::<AutoSize>()?.into_size_bytes(),
            15 * 1000 * 1000 * 1000
        );
        assert_eq!(
            "15TB".parse::<AutoSize>()?.into_size_bytes(),
            15 * 1000 * 1000 * 1000 * 1000
        );

        assert_eq!("15KB".into_size_bytes(), 15 * 1024);

        assert_eq!("15.2KB".into_size_bytes(), 15564);

        assert_eq!("0.0MB".into_size_bytes(), 0);
        assert_eq!("0.0KB".into_size_bytes(), 0);
        Ok(())
    }

    #[test]
    #[should_panic(expected = "invalid size")]
    fn test_invalid_size() {
        "45tg5g6h".into_size_bytes();
    }
}
